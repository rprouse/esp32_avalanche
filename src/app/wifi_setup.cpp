#include "wifi_setup.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <algorithm>
#include <atomic>
#include <vector>

#include "display/display_port.h"
#include "net/wifi_creds.h"
#include "ui/setup_screen.h"

namespace wifi_setup {
namespace {

constexpr uint32_t kIdleTimeoutMs = 10 * 60 * 1000;
constexpr uint32_t kConnectTimeoutMs = 20000;
constexpr uint32_t kRestartDelayMs = 4000;  // lets the page fetch the final status first
const IPAddress kApIp(192, 168, 4, 1);
constexpr const char *kUrl = "http://192.168.4.1";

enum class State { Idle, Connecting, Connected, Failed };

String s_apSsid;
String s_apPass;
bool s_haveCreds = false;
String s_currentSsid;  // saved/configured network, shown on the page (never the password)

State s_state = State::Idle;
String s_message;
String s_trySsid, s_tryPass;
uint32_t s_connectStart = 0;
uint32_t s_lastActivity = 0;
uint32_t s_restartAt = 0;
String s_scanJson = "[]";

std::atomic<uint8_t> s_lastReason{0};
std::atomic<int> s_disconnects{0};

DNSServer s_dns;
WebServer s_server(80);

// ---------------------------------------------------------------- page

const char kPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Backcountry Wi-Fi setup</title>
<style>
body{font-family:system-ui,-apple-system,sans-serif;background:#10161f;color:#d8e0e8;margin:0;padding:16px}
main{max-width:420px;margin:auto}
h1{font-size:1.5em;color:#fff;margin:8px 0 4px}
p{margin:4px 0 12px}
label{display:block;margin:18px 0 6px;color:#9aa8b8}
select,input,button{width:100%;box-sizing:border-box;font-size:1.05em;padding:12px;border-radius:8px;border:1px solid #2a3a4d;background:#1c2633;color:#fff}
button{background:#4fa3e0;border:0;color:#10161f;font-weight:600;margin-top:22px}
button:disabled{opacity:.5}
.row{display:flex;gap:8px}.row select{flex:1}.row button{width:auto;margin:0;padding:12px 16px}
.chk{display:flex;gap:8px;align-items:center;margin-top:10px;color:#9aa8b8}.chk input{width:auto}
#status{margin-top:18px;padding:12px;border-radius:8px;display:none}
.info{background:#1c2633}.ok{background:#1f3b26;color:#a8e6ab}.err{background:#4a2a12;color:#ffc58a}
small{color:#9aa8b8}
.link{background:none;color:#9aa8b8;font-weight:normal;text-decoration:underline;margin-top:28px}
</style></head><body><main>
<h1>Wi-Fi setup</h1>
<p id="cur"></p>
<form id="f">
<label for="net">Network</label>
<div class="row"><select id="net"><option>Scanning...</option></select><button type="button" id="rescan" aria-label="Scan again">&#x21bb;</button></div>
<div id="other" hidden><label for="ssid">Network name</label><input id="ssid" maxlength="32" autocapitalize="none" autocorrect="off" spellcheck="false"></div>
<label for="pass">Password</label>
<input id="pass" type="password" maxlength="63" autocapitalize="none" autocorrect="off" spellcheck="false">
<label class="chk"><input type="checkbox" id="show"> Show password</label>
<button id="go">Connect</button>
</form>
<div id="status"></div>
<p style="margin-top:16px"><small>2.4 GHz networks only. The display can't use 5 GHz Wi-Fi.</small></p>
<button class="link" id="forget" hidden>Forget saved Wi-Fi</button>
</main><script>
const $=i=>document.getElementById(i),OTHER='\u0000other';
let nets=[],busy=false;
function show(t,c){const s=$('status');s.textContent=t;s.className=c;s.style.display='block'}
function pickOther(){$('other').hidden=$('net').value!==OTHER}
async function scan(refresh){
  const sel=$('net');sel.innerHTML='<option>Scanning...</option>';
  try{
    const j=await (await fetch('/scan.json'+(refresh?'?refresh=1':''))).json();
    nets=j.networks;sel.innerHTML='';
    for(const n of nets){const o=document.createElement('option');o.value=n.ssid;
      o.textContent=n.ssid+(n.secure?'':' (open)')+(n.rssi<-75?' - weak':'');sel.appendChild(o)}
    const o=document.createElement('option');o.value=OTHER;o.textContent='Other network...';sel.appendChild(o);
    if(j.current){$('cur').textContent='Currently using: '+j.current;if(nets.some(n=>n.ssid===j.current))sel.value=j.current}
    else $('cur').textContent='Choose the Wi-Fi network the display should use.';
    $('forget').hidden=!j.saved;
  }catch(e){sel.innerHTML='';const o=document.createElement('option');o.value=OTHER;o.textContent='Other network...';sel.appendChild(o)}
  pickOther();
}
async function poll(){
  let j;
  try{j=await (await fetch('/status.json',{cache:'no-store'})).json()}
  catch(e){show('Lost contact with the display while it connects. Check its screen.','info');setTimeout(poll,1500);return}
  if(j.state==='connecting'){show(j.message,'info');setTimeout(poll,1000)}
  else if(j.state==='connected'){show(j.message,'ok')}
  else{show(j.message||'Failed','err');busy=false;$('go').disabled=false}
}
$('net').onchange=pickOther;
$('rescan').onclick=()=>scan(true);
$('show').onchange=e=>$('pass').type=e.target.checked?'text':'password';
$('f').onsubmit=async e=>{
  e.preventDefault();if(busy)return;
  const other=$('net').value===OTHER,ssid=other?$('ssid').value:$('net').value,pass=$('pass').value;
  if(!ssid){show('Choose a network.','err');return}
  if(pass&&(pass.length<8||pass.length>63)){show('Wi-Fi passwords are 8 to 63 characters.','err');return}
  const n=nets.find(n=>n.ssid===ssid);
  if(!pass&&(!n||n.secure)&&!confirm('Connect without a password?'))return;
  busy=true;$('go').disabled=true;show('Connecting to '+ssid+'...','info');
  try{
    const r=await fetch('/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:'ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass)});
    if(!r.ok){show(await r.text(),'err');busy=false;$('go').disabled=false;return}
  }catch(e){}
  setTimeout(poll,1000);
};
$('forget').onclick=async()=>{
  if(!confirm('Forget the saved Wi-Fi network? The display restarts.'))return;
  try{await fetch('/forget',{method:'POST'})}catch(e){}
  show('Forgotten. The display is restarting.','ok');
};
scan(false);
</script></body></html>)HTML";

// ---------------------------------------------------------------- helpers

void screenStatus(const String &text, ui::StatusLevel level) {
    log_i("Setup: %s", text.c_str());
    display::Lock lock;
    setup_screen::setStatus(text.c_str(), level);
}

String jsonEscape(const String &s) {
    String out;
    out.reserve(s.length() + 2);
    for (size_t i = 0; i < s.length(); ++i) {
        const char c = s[i];
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if ((uint8_t)c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof buf, "\\u%04x", (unsigned)(uint8_t)c);
            out += buf;
        } else {
            out += c;
        }
    }
    return out;
}

/** Blocking scan (a few seconds). Keeps the strongest entry per SSID, hides hidden networks. */
void scanNetworks() {
    struct Net {
        String ssid;
        int32_t rssi;
        bool secure;
    };
    std::vector<Net> nets;
    const int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i) {
        const String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) continue;
        auto it = std::find_if(nets.begin(), nets.end(), [&](const Net &x) { return x.ssid == ssid; });
        if (it == nets.end()) nets.push_back({ssid, WiFi.RSSI(i), WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
        else if (WiFi.RSSI(i) > it->rssi) it->rssi = WiFi.RSSI(i);
    }
    WiFi.scanDelete();
    std::sort(nets.begin(), nets.end(), [](const Net &a, const Net &b) { return a.rssi > b.rssi; });
    if (nets.size() > 25) nets.resize(25);

    String json = "[";
    for (size_t i = 0; i < nets.size(); ++i) {
        if (i) json += ',';
        json += "{\"ssid\":\"" + jsonEscape(nets[i].ssid) + "\",\"rssi\":" + String(nets[i].rssi) +
                ",\"secure\":" + (nets[i].secure ? "true" : "false") + "}";
    }
    json += "]";
    s_scanJson = json;
    log_i("Setup: found %u networks", (unsigned)nets.size());
}

String failureMessage(const String &ssid, uint8_t reason) {
    switch (reason) {
        case WIFI_REASON_NO_AP_FOUND:
        case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
        case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
        case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
            return "Couldn't find " + ssid + ". Is it a 2.4 GHz network, and in range?";
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_MIC_FAILURE:
            return "Couldn't join " + ssid + " - check the password.";
        default:
            return "Couldn't connect to " + ssid + ".";
    }
}

bool definitiveFailure(uint8_t reason) {
    switch (reason) {
        case WIFI_REASON_NO_AP_FOUND:
        case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
        case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
        case WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD:
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return true;
        default:
            return false;
    }
}

// ---------------------------------------------------------------- HTTP handlers

void touch() { s_lastActivity = millis(); }

bool isOurHost() {
    const String host = s_server.hostHeader();
    return host.isEmpty() || host == kApIp.toString() || host.startsWith(kApIp.toString() + ":");
}

void handleRoot() {
    touch();
    s_server.sendHeader("Cache-Control", "no-store");
    s_server.send_P(200, "text/html", kPage);
}

void handleScan() {
    touch();
    if (s_server.hasArg("refresh") && s_state != State::Connecting) scanNetworks();
    String json = "{\"networks\":" + s_scanJson + ",\"current\":\"" + jsonEscape(s_currentSsid) +
                  "\",\"saved\":" + (s_haveCreds && wifi_creds::load().saved ? "true" : "false") + "}";
    s_server.sendHeader("Cache-Control", "no-store");
    s_server.send(200, "application/json", json);
}

void handleStatus() {
    touch();
    const char *state = "idle";
    switch (s_state) {
        case State::Idle: state = "idle"; break;
        case State::Connecting: state = "connecting"; break;
        case State::Connected: state = "connected"; break;
        case State::Failed: state = "failed"; break;
    }
    s_server.sendHeader("Cache-Control", "no-store");
    s_server.send(200, "application/json",
                  String("{\"state\":\"") + state + "\",\"message\":\"" + jsonEscape(s_message) + "\"}");
}

void handleSave() {
    touch();
    if (s_state == State::Connecting || s_state == State::Connected) {
        s_server.send(409, "text/plain", "Already connecting - please wait.");
        return;
    }
    const String ssid = s_server.arg("ssid");
    const String pass = s_server.arg("pass");
    if (ssid.isEmpty() || ssid.length() > 32) {
        s_server.send(400, "text/plain", "Network names are 1 to 32 characters.");
        return;
    }
    if (!pass.isEmpty() && (pass.length() < 8 || pass.length() > 63)) {
        s_server.send(400, "text/plain", "Wi-Fi passwords are 8 to 63 characters.");
        return;
    }
    s_trySsid = ssid;
    s_tryPass = pass;
    s_lastReason = 0;
    s_disconnects = 0;
    WiFi.disconnect();
    WiFi.begin(s_trySsid.c_str(), s_tryPass.isEmpty() ? nullptr : s_tryPass.c_str());
    s_connectStart = millis();
    s_state = State::Connecting;
    s_message = "Connecting to " + ssid + "...";
    screenStatus(s_message, ui::StatusLevel::Info);
    s_server.send(200, "text/plain", "OK");
}

void handleForget() {
    touch();
    wifi_creds::forget();
    s_server.send(200, "text/plain", "OK");
    s_state = State::Connected;  // stops further saves
    s_message = "Saved Wi-Fi forgotten. Restarting...";
    screenStatus(s_message, ui::StatusLevel::Info);
    s_restartAt = millis() + 2000;
}

/** Captive portal: anything for another host goes to our page (this triggers the phone's sign-in sheet). */
void handleNotFound() {
    touch();
    if (!isOurHost()) {
        s_server.sendHeader("Location", String(kUrl) + "/", true);
        s_server.send(302, "text/plain", "");
        return;
    }
    s_server.send(404, "text/plain", "Not found");
}

// ---------------------------------------------------------------- task

void pollConnect() {
    if (s_state != State::Connecting) return;
    if (WiFi.status() == WL_CONNECTED) {
        log_i("Setup: connected to \"%s\", IP %s", s_trySsid.c_str(), WiFi.localIP().toString().c_str());
        wifi_creds::save(s_trySsid, s_tryPass);
        s_state = State::Connected;
        s_message = "Connected to " + s_trySsid + ". The display is restarting.";
        screenStatus("Connected to " + s_trySsid + "! Restarting...", ui::StatusLevel::Ok);
        s_restartAt = millis() + kRestartDelayMs;
        return;
    }
    // The driver retries once after the first failure; two definite failures (or the timeout) = give up.
    const bool gaveUp = s_disconnects >= 2 && definitiveFailure(s_lastReason);
    if (gaveUp || millis() - s_connectStart > kConnectTimeoutMs) {
        log_w("Setup: connecting to \"%s\" failed, reason %u", s_trySsid.c_str(), (unsigned)s_lastReason.load());
        WiFi.disconnect();
        s_state = State::Failed;
        s_message = failureMessage(s_trySsid, s_lastReason);
        screenStatus(s_message, ui::StatusLevel::Error);
        s_tryPass = "";
    }
}

void task(void *) {
    screenStatus("Looking for Wi-Fi networks...", ui::StatusLevel::Info);
    scanNetworks();

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(kApIp, kApIp, IPAddress(255, 255, 255, 0));
    if (!WiFi.softAP(s_apSsid.c_str(), s_apPass.c_str())) {
        screenStatus("Couldn't start the setup network. Restarting...", ui::StatusLevel::Error);
        delay(5000);
        esp_restart();
    }
    s_dns.setErrorReplyCode(DNSReplyCode::NoError);
    s_dns.start(53, "*", kApIp);

    s_server.on("/", HTTP_GET, handleRoot);
    s_server.on("/scan.json", HTTP_GET, handleScan);
    s_server.on("/status.json", HTTP_GET, handleStatus);
    s_server.on("/save", HTTP_POST, handleSave);
    s_server.on("/forget", HTTP_POST, handleForget);
    s_server.onNotFound(handleNotFound);
    s_server.begin();
    log_i("Setup: access point \"%s\" at %s", s_apSsid.c_str(), kApIp.toString().c_str());
    screenStatus("Waiting for a phone to connect...", ui::StatusLevel::Info);

    touch();
    int lastStations = 0;
    for (;;) {
        s_dns.processNextRequest();
        s_server.handleClient();
        pollConnect();

        const int stations = WiFi.softAPgetStationNum();
        if (stations != lastStations) {
            lastStations = stations;
            touch();
            if (s_state == State::Idle) {
                screenStatus(stations > 0 ? "Phone connected - waiting for the setup page..."
                                          : "Waiting for a phone to connect...",
                             ui::StatusLevel::Info);
            }
        }

        if (s_restartAt != 0 && (int32_t)(millis() - s_restartAt) >= 0) esp_restart();
        if (s_haveCreds && s_state != State::Connecting && s_restartAt == 0 &&
            millis() - s_lastActivity > kIdleTimeoutMs) {
            log_i("Setup: idle timeout, back to normal mode");
            esp_restart();
        }
        delay(5);
    }
}

}  // namespace

void start(bool haveCredentials) {
    s_haveCreds = haveCredentials;
    if (haveCredentials) s_currentSsid = wifi_creds::load().ssid;

    WiFi.persistent(false);  // credentials live in our own NVS namespace
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_STA);  // radio on: esp_random() is now truly random
    WiFi.onEvent(
        [](arduino_event_id_t, arduino_event_info_t info) {
            s_lastReason = info.wifi_sta_disconnected.reason;
            ++s_disconnects;
        },
        ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char ssid[24];
    snprintf(ssid, sizeof ssid, "Backcountry-%02X%02X", mac[4], mac[5]);
    s_apSsid = ssid;
    static const char kAlphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";  // no 0/o/1/l/i
    for (int i = 0; i < 8; ++i) s_apPass += kAlphabet[esp_random() % (sizeof kAlphabet - 1)];

    {
        display::Lock lock;
        // Cancel = restart; the one-shot setup request is already cleared, so it boots normally.
        void (*cancel)() = [] { esp_restart(); };
        setup_screen::build(s_apSsid.c_str(), s_apPass.c_str(), kUrl, haveCredentials ? cancel : nullptr);
    }
    xTaskCreatePinnedToCore(task, "wifi_setup", 12 * 1024, nullptr, 1, nullptr, 0);
}

}  // namespace wifi_setup
