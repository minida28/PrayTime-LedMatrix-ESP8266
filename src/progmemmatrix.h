#ifndef _PROGMEMMATRIX_H
#define _PROGMEMMATRIX_H

const char Page_WaitAndReload[] PROGMEM = R"=====(
<p>Rebooting...</p>
<p>Please wait in <strong><span id="counter">20</span></strong> second(s).</p>
<p>This page will refresh automatically.</p>
<script type="text/javascript">
function countdown() {
    var i = document.getElementById('counter');
    if (parseInt(i.textContent)<=0) {
        location.href = '/';
    }
    i.textContent = parseInt(i.textContent)-1;
}
setInterval(function(){ countdown(); },1000);
</script>
)=====";

const char fajrStr[] PROGMEM = "FAJR";
const char syuruqStr[] PROGMEM = "SYURUQ";
const char dhuhrStr[] PROGMEM = "DHUHR";
const char ashrStr[] PROGMEM = "ASHR";
const char sunsetStr[] PROGMEM = "SUNSET";
const char maghribStr[] PROGMEM = "MAGHRIB";
const char isyaStr[] PROGMEM = "ISYA";
const char sholatNameCountStr[] PROGMEM = "TIMESCOUNT";

const char *const sholatName_P[] PROGMEM =
    {
        fajrStr,
        syuruqStr,
        dhuhrStr,
        ashrStr,
        sunsetStr,
        maghribStr,
        isyaStr,

        sholatNameCountStr};

const char pgm_txt_manual[] PROGMEM = "manual";
const char pgm_txt_hapelaptop[] PROGMEM = "hapelaptop";
const char pgm_txt_timeis[] PROGMEM = "timeis";
const char pgm_txt_syncChoice[] PROGMEM = "syncChoice";

const char pgm_txt_gotyourtextmessage[] PROGMEM = "I got your text message";
const char pgm_txt_gotyourbinarymessage[] PROGMEM = "I got your binary message";
static const char pgm_txt_serverloseconnection[] PROGMEM = "Server close connection";

const char Page_Restart[] PROGMEM = "Please Wait... Configuring and Restarting";
const char Page_Reset[] PROGMEM = "Please Wait... Configuring and Resetting";
const char modelName[] PROGMEM = "ESP8266EX";
const char modelNumber[] PROGMEM = "929000226503";

//network
const char WL_IDLE_STATUS_Str[] PROGMEM = "WL_IDLE_STATUS";
const char WL_NO_SSID_AVAIL_Str[] PROGMEM = "WL_NO_SSID_AVAIL";
const char WL_SCAN_COMPLETED_Str[] PROGMEM = "WL_SCAN_COMPLETED";
const char WL_CONNECTED_Str[] PROGMEM = "WL_CONNECTED";
const char WL_CONNECT_FAILED_Str[] PROGMEM = "WL_CONNECT_FAILED";
const char WL_CONNECTION_LOST_Str[] PROGMEM = "WL_CONNECTION_LOST";
const char WL_DISCONNECTED_Str[] PROGMEM = "WL_DISCONNECTED";

const char *const wifistatus_P[] PROGMEM =
    {
        //   WL_NO_SHIELD_Str,
        WL_IDLE_STATUS_Str,
        WL_NO_SSID_AVAIL_Str,
        WL_SCAN_COMPLETED_Str,
        WL_CONNECTED_Str,
        WL_CONNECT_FAILED_Str,
        WL_CONNECTION_LOST_Str,
        WL_DISCONNECTED_Str};

const char NULL_Str[] PROGMEM = "NULL";
const char STA_Str[] PROGMEM = "STA";
const char AP_Str[] PROGMEM = "AP";
const char STA_AP_Str[] PROGMEM = "STA+AP";

const char *const wifimode_P[] PROGMEM =
    {
        // WL_NO_SHIELD_Str,
        NULL_Str,
        STA_Str,
        AP_Str,
        STA_AP_Str};

const char pgm_provincedb_file[] PROGMEM = "/provinces.csv";
const char pgm_regencydb_file[] PROGMEM = "/regencies.csv";
const char pgm_districtdb_file[] PROGMEM = "/districts.csv";
const char pgm_regenciestartpos_file[] PROGMEM = "/regenciesList.csv";
const char pgm_districtstartpos_file[] PROGMEM = "/districtsList.csv";

const char pgm_loc[] PROGMEM = "loc";
const char pgm_hostname[] PROGMEM = "hostname";
const char pgm_mode[] PROGMEM = "mode";
const char pgm_ssid[] PROGMEM = "ssid";
const char pgm_password[] PROGMEM = "password";
const char pgm_encryption[] PROGMEM = "encryption";
const char pgm_dhcp[] PROGMEM = "dhcp";
const char pgm_static_ip[] PROGMEM = "static_ip";
const char pgm_sta_ip[] PROGMEM = "sta_ip";
const char pgm_sta_mac[] PROGMEM = "sta_mac";
const char pgm_ap_ip[] PROGMEM = "ap_ip";
const char pgm_ap_mac[] PROGMEM = "ap_mac";
const char pgm_gateway[] PROGMEM = "gateway";
const char pgm_netmask[] PROGMEM = "netmask";
const char pgm_mac[] PROGMEM = "mac";
const char pgm_dns0[] PROGMEM = "dns0";
const char pgm_dns1[] PROGMEM = "dns1";
const char pgm_rssi[] PROGMEM = "rssi";
const char pgm_isconnected[] PROGMEM = "isconnected";
const char pgm_autoconnect[] PROGMEM = "autoconnect";
const char pgm_persistent[] PROGMEM = "persistent";
const char pgm_bssid[] PROGMEM = "bssid";
const char pgm_status[] PROGMEM = "status";
const char pgm_phymode[] PROGMEM = "phymode";
const char pgm_channel[] PROGMEM = "channel";

// System Info
const char pgm_chipid[] PROGMEM = "chipid";
const char pgm_cpufreq[] PROGMEM = "cpufreq";
const char pgm_sketchsize[] PROGMEM = "sketchsize";
const char pgm_freesketchspace[] PROGMEM = "freesketchspace";
const char pgm_flashchipid[] PROGMEM = "flashchipid";
const char pgm_flashchipmode[] PROGMEM = "flashchipmode";
const char pgm_flashchipsize[] PROGMEM = "flashchipsize";
const char pgm_flashchiprealsize[] PROGMEM = "flashchiprealsize";
const char pgm_flashchipspeed[] PROGMEM = "flashchipspeed";
const char pgm_cyclecount[] PROGMEM = "cyclecount";
const char pgm_corever[] PROGMEM = "corever";
const char pgm_sdkver[] PROGMEM = "sdkver";
const char pgm_bootmode[] PROGMEM = "bootmode";
const char pgm_bootversion[] PROGMEM = "bootversion";
const char pgm_resetreason[] PROGMEM = "resetreason";
const char pgm_filename[] PROGMEM = "filename";
const char pgm_compiledate[] PROGMEM = "compiledate";
const char pgm_compiletime[] PROGMEM = "compiletime";

const char pgm_WIFI_AP[] PROGMEM = "WIFI_AP";
const char pgm_WIFI_STA[] PROGMEM = "WIFI_STA";
const char pgm_WIFI_AP_STA[] PROGMEM = "WIFI_AP_STA";
const char pgm_WIFI_OFF[] PROGMEM = "WIFI_OFF";
const char pgm_NA[] PROGMEM = "N/A";

const char pgm_never[] PROGMEM = "NEVER";
const char pgm_txt_infoStatic[] PROGMEM = "infoStatic";
const char pgm_infodynamic[] PROGMEM = "infoDynamic";
const char pgm_sholatdynamic[] PROGMEM = "sholatDynamic";
const char pgm_sholatstatic[] PROGMEM = "sholatStatic";
const char pgm_matrixconfig[] PROGMEM = "matrixConfig";
const char pgm_automatic[] PROGMEM = "Automatic";
const char pgm_manual[] PROGMEM = "Manual";

const char pgm_url[] PROGMEM = "url";
const char pgm_param[] PROGMEM = "param";
const char pgm_value[] PROGMEM = "value";
const char pgm_text[] PROGMEM = "text";
const char pgm_type[] PROGMEM = "type";
const char pgm_descriptionxmlfile[] PROGMEM = "/description.xml";
const char pgm_systeminfofile[] PROGMEM = "/systeminfo.json";
const char pgm_statuspagesystem[] PROGMEM = "/status.html";
const char pgm_saveconfig[] PROGMEM = "saveconfig";
const char pgm_configpagelocation[] PROGMEM = "/configlocation.html";
const char pgm_configfilelocation[] PROGMEM = "/configlocation.json";
const char pgm_configpagenetwork[] PROGMEM = "/confignetwork.html";
const char pgm_configfilenetwork[] PROGMEM = "/confignetwork.json";
const char pgm_statuspagenetwork[] PROGMEM = "/statusnetwork.html";
const char pgm_configpagetime[] PROGMEM = "/configtime.html";
const char pgm_configfiletime[] PROGMEM = "/configtime.json";
const char pgm_statuspagetime[] PROGMEM = "/statustime.html";
const char pgm_configpageledmatrix[] PROGMEM = "/configledmatrix.html";
const char pgm_configfileledmatrix[] PROGMEM = "/configledmatrix.json";
const char pgm_configpagesholat[] PROGMEM = "/configsholat.html";
const char pgm_configfilesholat[] PROGMEM = "/configsholat.json";
const char pgm_schedulepagesholat[] PROGMEM = "/sholat.html";
const char pgm_configpagemqtt[] PROGMEM = "/configmqtt.html";
const char pgm_configfilemqtt[] PROGMEM = "/configmqtt.json";
const char pgm_statuspagemqtt[] PROGMEM = "/statusmqtt.html";
const char pgm_configpagemqttpubsub[] PROGMEM = "configPubSub";
const char pgm_configfilemqttpubsub[] PROGMEM = "/configmqttpubsub.json";
const char pgm_settimepage[] PROGMEM = "/setrtctime.html";

const char pgm_matrixConfig[] PROGMEM = "matrixConfig";
const char pgm_pagemode0[] PROGMEM = "pagemode0";
const char pgm_pagemode1[] PROGMEM = "pagemode1";
const char pgm_pagemode2[] PROGMEM = "pagemode2";
const char pgm_scrollrow_0[] PROGMEM = "scrollrow_0";
const char pgm_scrollrow_1[] PROGMEM = "scrollrow_1";
const char pgm_scrollspeed[] PROGMEM = "scrollspeed";
const char pgm_scrollspeedslider[] PROGMEM = "scrollspeedslider";
const char pgm_scrollspeedvalue[] PROGMEM = "scrollspeedvalue";
const char pgm_brightness[] PROGMEM = "brightness";
const char pgm_brightnessslider[] PROGMEM = "brightnessslider";
const char pgm_brightnessvalue[] PROGMEM = "brightnessvalue";
const char pgm_operatingmode[] PROGMEM = "operatingmode";
const char pgm_pagemode[] PROGMEM = "pagemode";
const char pgm_adzanwaittime[] PROGMEM = "adzanwaittime";
const char pgm_iqamahwaittime[] PROGMEM = "iqamahwaittime";

const char pgm_true[] PROGMEM = "true";
const char pgm_longtext[] PROGMEM = "longtext";
const char pgm_btntesttone0[] PROGMEM = "btntesttone0";
const char pgm_btntesttone1[] PROGMEM = "btntesttone1";
const char pgm_btntesttone2[] PROGMEM = "btntesttone2";
const char pgm_btntesttone10[] PROGMEM = "btntesttone10";
const char pgm_btnsaveconfig[] PROGMEM = "btnsaveconfig";

//sholat
const char pgm_day[] PROGMEM = "day";
const char pgm_fajr[] PROGMEM = "fajr";
const char pgm_syuruq[] PROGMEM = "syuruq";
const char pgm_dhuhr[] PROGMEM = "dhuhr";
const char pgm_ashr[] PROGMEM = "ashr";
const char pgm_maghrib[] PROGMEM = "maghrib";
const char pgm_isya[] PROGMEM = "isya";
const char pgm_h[] PROGMEM = "h";
const char pgm_m[] PROGMEM = "m";
const char pgm_s[] PROGMEM = "s";
const char pgm_curr[] PROGMEM = "curr";
const char pgm_next[] PROGMEM = "next";
const char pgm_sholatConfig[] PROGMEM = "sholatConfig";
const char pgm_location[] PROGMEM = "location";
const char pgm_calcMethod[] PROGMEM = "calcMethod";
const char pgm_Jafari[] PROGMEM = "Jafari";
const char pgm_Karachi[] PROGMEM = "Karachi";
const char pgm_ISNA[] PROGMEM = "ISNA";
const char pgm_MWL[] PROGMEM = "MWL";
const char pgm_Makkah[] PROGMEM = "Makkah";
const char pgm_Egypt[] PROGMEM = "Egypt";
const char pgm_Custom[] PROGMEM = "Custom";
const char pgm_asrJuristic[] PROGMEM = "asrJuristic";
const char pgm_Shafii[] PROGMEM = "Shafii";
const char pgm_Hanafi[] PROGMEM = "Hanafi";
const char pgm_highLatsAdjustMethod[] PROGMEM = "highLatsAdjustMethod";
const char pgm_None[] PROGMEM = "None";
const char pgm_MidNight[] PROGMEM = "MidNight";
const char pgm_OneSeventh[] PROGMEM = "OneSeventh";
const char pgm_AngleBased[] PROGMEM = "AngleBased";
const char pgm_fajrAngle[] PROGMEM = "fajrAngle";
const char pgm_maghribAngle[] PROGMEM = "maghribAngle";
const char pgm_ishaAngle[] PROGMEM = "ishaAngle";
const char pgm_offsetImsak[] PROGMEM = "offsetImsak";
const char pgm_offsetFajr[] PROGMEM = "offsetFajr";
const char pgm_offsetSunrise[] PROGMEM = "offsetSunrise";
const char pgm_offsetDhuhr[] PROGMEM = "offsetDhuhr";
const char pgm_offsetAsr[] PROGMEM = "offsetAsr";
const char pgm_offsetSunset[] PROGMEM = "offsetSunset";
const char pgm_offsetMaghrib[] PROGMEM = "offsetMaghrib";
const char pgm_offsetIsha[] PROGMEM = "offsetIsha";

//time
const char pgm_time[] PROGMEM = "time";
const char pgm_date[] PROGMEM = "date";
const char pgm_lastboot[] PROGMEM = "lastboot";
const char pgm_lastsync[] PROGMEM = "lastsync";
const char pgm_nextsync[] PROGMEM = "nextsync";
const char pgm_uptime[] PROGMEM = "uptime";
const char pgm_enablentp[] PROGMEM = "enablentp";
const char pgm_enablertc[] PROGMEM = "enablertc";

const char pgm_province[] PROGMEM = "province";
const char pgm_regency[] PROGMEM = "regency";
const char pgm_district[] PROGMEM = "district";
const char pgm_timezone[] PROGMEM = "timezone";
const char pgm_timezonestring[] PROGMEM = "timezonestring";
const char pgm_latitude[] PROGMEM = "latitude";
const char pgm_longitude[] PROGMEM = "longitude";
const char pgm_dst[] PROGMEM = "dst";
const char pgm_ntpserver_0[] PROGMEM = "ntpserver_0";
const char pgm_ntpserver_1[] PROGMEM = "ntpserver_1";
const char pgm_ntpserver_2[] PROGMEM = "ntpserver_2";
const char pgm_syncinterval[] PROGMEM = "syncinterval";

//system
const char pgm_heapstart[] PROGMEM = "heapstart";
const char pgm_heapend[] PROGMEM = "heapend";
const char pgm_heap[] PROGMEM = "heap";
const char pgm_freeheap[] PROGMEM = "freeheap";

//SPIIFS
const char pgm_totalbytes[] PROGMEM = "totalbytes";
const char pgm_usedbytes[] PROGMEM = "usedbytes";
const char pgm_blocksize[] PROGMEM = "blocksize";
const char pgm_pagesize[] PROGMEM = "pagesize";
const char pgm_maxopenfiles[] PROGMEM = "maxopenfiles";
const char pgm_maxpathlength[] PROGMEM = "maxpathlength";

//temp sensor
const char pgm_roomtemp[] PROGMEM = "roomtemp";

//rtc
const char pgm_rtctemp[] PROGMEM = "rtctemp";

//SSDP
const char pgm_descriptionxml[] PROGMEM = "description.xml";
const char pgm_upnprootdevice[] PROGMEM = "upnp:rootdevice";

//HTTP Auth
const char pgm_wwwauth[] PROGMEM = "wwwauth";
const char pgm_wwwuser[] PROGMEM = "wwwuser";
const char pgm_wwwpass[] PROGMEM = "wwwpass";

// Others
const char pgm_startingup[] PROGMEM = "Starting-up...";
const char pgm_loading[] PROGMEM = "Loading...";

#endif
