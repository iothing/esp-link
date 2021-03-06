
#include <esp8266.h>
#include "cgi.h"
#include "espfs.h"
#include "config.h"
#include "serled.h"
#include "status.h"
#include "serbridge.h"

static char *map_names[] = {
  "esp-bridge", "jn-esp-v2", "esp-01(AVR)", "esp-01(ARM)", "esp-br-rev",
};
static char* map_func[] = { "reset", "isp", "conn_led", "ser_led" };
static int8_t map_asn[][4] = {
  { 12, 13,  0, 14 },  // esp-bridge
  { 12, 13,  0,  2 },  // jn-esp-v2
  {  0, -1,  2, -1 },  // esp-01(AVR)
  {  0,  2, -1, -1 },  // esp-01(ARM)
  { 13, 12, 14,  0 },  // esp-br-rev -- for test purposes
};
static const int num_map_names = sizeof(map_names)/sizeof(char*);
static const int num_map_func = sizeof(map_func)/sizeof(char*);

// Cgi to return choice of pin assignments
int ICACHE_FLASH_ATTR cgiPinsGet(HttpdConnData *connData) {
	char buff[1024];
  int len;

	if (connData->conn==NULL) {
		return HTTPD_CGI_DONE; // Connection aborted
	}

  // figure out current mapping
  int curr = 99;
  for (int i=0; i<num_map_names; i++) {
    int8_t *map = map_asn[i];
    if (map[0] == flashConfig.reset_pin && map[1] == flashConfig.isp_pin &&
        map[2] == flashConfig.conn_led_pin && map[3] == flashConfig.ser_led_pin) {
      curr = i;
    }
  }

  len = os_sprintf(buff, "{ \"curr\":\"%s\", \"map\": [ ", map_names[curr]);
  for (int i=0; i<num_map_names; i++) {
    if (i != 0) buff[len++] = ',';
    len += os_sprintf(buff+len, "\n{ \"value\":%d, \"name\":\"%s\"", i, map_names[i]);
    for (int f=0; f<num_map_func; f++) {
      len += os_sprintf(buff+len, ", \"%s\":%d", map_func[f], map_asn[i][f]);
    }
    len += os_sprintf(buff+len, ", \"descr\":\"");
    for (int f=0; f<num_map_func; f++) {
      int8_t p = map_asn[i][f];
      if (p >= 0) len += os_sprintf(buff+len, " %s:gpio%d", map_func[f], p);
      else len += os_sprintf(buff+len, " %s:n/a", map_func[f]);
    }
    len += os_sprintf(buff+len, "\" }");
  }
  len += os_sprintf(buff+len, "\n] }");

	jsonHeader(connData, 200);
	httpdSend(connData, buff, len);
	return HTTPD_CGI_DONE;
}

// Cgi to change choice of pin assignments
int ICACHE_FLASH_ATTR cgiPinsSet(HttpdConnData *connData) {
	if (connData->conn==NULL) {
		return HTTPD_CGI_DONE; // Connection aborted
	}

  char buff[128];
	int len = httpdFindArg(connData->getArgs, "map", buff, sizeof(buff));
	if (len <= 0) {
	  jsonHeader(connData, 400);
    return HTTPD_CGI_DONE;
  }

  int m = atoi(buff);
	if (m < 0 || m >= num_map_names) {
	  jsonHeader(connData, 400);
    return HTTPD_CGI_DONE;
  }

  os_printf("Switching pin map to %s (%d)\n", map_names[m], m);
  int8_t *map = map_asn[m];
  flashConfig.reset_pin    = map[0];
  flashConfig.isp_pin      = map[1];
  flashConfig.conn_led_pin = map[2];
  flashConfig.ser_led_pin  = map[3];

  serbridgeInitPins();
  serledInit();
  statusInit();

  if (configSave()) {
    os_printf("New config saved\n");
    httpdStartResponse(connData, 200);
    httpdEndHeaders(connData);
  } else {
    os_printf("*** Failed to save config ***\n");
    httpdStartResponse(connData, 500);
    httpdEndHeaders(connData);
    httpdSend(connData, "Failed to save config", -1);
  }
	return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiPins(HttpdConnData *connData) {
	if (connData->requestType == HTTPD_METHOD_GET) {
		return cgiPinsGet(connData);
	} else if (connData->requestType == HTTPD_METHOD_POST) {
		return cgiPinsSet(connData);
	} else {
		jsonHeader(connData, 404);
		return HTTPD_CGI_DONE;
	}
}
