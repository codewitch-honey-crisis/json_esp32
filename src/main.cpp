#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <json.hpp>
using namespace io;
using namespace json;
void indent(int tabs) {
    while(tabs--) Serial.print("  ");
}
template<typename Reader>
void dump(Reader& reader) {
// don't de-escape and dequote field names or string values:
    //reader.raw_strings(true);
    bool first_part=true;
    int tabs = 0;
    bool skip_read = false;
    while(skip_read || reader.read()) {
        skip_read = false;
        switch(reader.node_type()) {
            case json_node_type::array:
                indent(tabs++);
                Serial.println("[");
                break;
            case json_node_type::end_array:
                indent(--tabs);
                Serial.println("]");
                break;
            case json_node_type::object:
                indent(tabs++);
                Serial.println("{");
                break;
            case json_node_type::end_object:
                indent(--tabs);
                Serial.println("}");
                break;
            case json_node_type::field:
                indent(tabs);
                Serial.printf("%s: ",reader.value());
                while(reader.read() && reader.is_value()) {
                    Serial.printf("%s",reader.value());
                }
                Serial.println("");
                skip_read = true;
                break;
            case json_node_type::value:
                indent(tabs);
                Serial.printf("%s\r\n",reader.value());
                break;
            case json_node_type::value_part:
                if(first_part) {
                    indent(tabs);
                    first_part = false;
                }
                Serial.printf("%s",reader.value());
                break;
            case json_node_type::end_value_part:
                Serial.printf("%s,\r\n",reader.value());               
                first_part = true;
                break;      
        }
    }

}
void setup() {
    Serial.begin(115200);
    constexpr static const char* wtime_url = 
        "http://worldtimeapi.org/api/ip";

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.begin();
    while(WiFi.status()!=WL_CONNECTED) {
        delay(10);
    }
    HTTPClient client;
    client.begin(wtime_url);
    if(0>=client.GET()) {
        while(1);
    }
    WiFiClient& www_client = client.getStream();
    arduino_stream www_stm(&www_client);
    json_reader www_reader(www_stm);
    dump(www_reader);
    WiFi.disconnect();
    if(SPIFFS.begin(false)) {
        // use binary mode in case UTF-8
        File file = SPIFFS.open("/data.json","rb");    
        if(!file) {
            return;
        }
        file_stream file_stm(file);
        json_reader file_reader(file_stm);
        dump(file_reader);
        file_stm.close();
    }
}
void loop() {

}