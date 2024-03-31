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
// accept any reader regardless of capture size
void dump(json_reader_base& reader, Stream& output) {
    // don't de-escape and dequote field names or string values:
    //reader.raw_strings(true);
    
    bool first_part=true; // first value part in series
    int tabs = 0; // number of "tabs" to indent by
    bool skip_read = false; // don't call read() the next iteration
    while(skip_read || reader.read()) {
        skip_read = false;
        switch(reader.node_type()) {
            case json_node_type::array:
                indent(tabs++);
                output.println("[");
                break;
            case json_node_type::end_array:
                indent(--tabs);
                output.println("]");
                break;
            case json_node_type::object:
                indent(tabs++);
                output.println("{");
                break;
            case json_node_type::end_object:
                indent(--tabs);
                output.println("}");
                break;
            case json_node_type::field:
                indent(tabs);
                output.printf("%s: ",reader.value());
                // we want to spit the value here, so 
                // we basically hijack the reader and 
                // read the value subtree here.
                while(reader.read() && reader.is_value()) {
                    output.printf("%s",reader.value());
                }
                output.println("");
                skip_read = true;
                break;
            case json_node_type::value:
                indent(tabs);
                output.printf("%s\r\n",reader.value());
                break;
            case json_node_type::value_part:
                // the first value part needs to be indented
                if(first_part) {
                    indent(tabs);
                    first_part = false; // reset the flag
                }
                output.printf("%s",reader.value());
                break;
            case json_node_type::end_value_part:
                output.printf("%s,\r\n",reader.value());               
                // set the first flag
                first_part = true;
                break;      
        }
    }
}
char name[2048];
char overview[8192];
void read_episodes(json_reader_base& reader, Stream& output) {
    int root_array_depth = 0;
    // episodes opens with an array
    if(reader.read() && reader.node_type()==json_node_type::array) {
        root_array_depth = reader.depth();
        while(true) {
            // if we're at the end of the array, break
            if(reader.depth()==root_array_depth && 
                reader.node_type()==json_node_type::end_array) {
                break;
            }
            // read each "episode object"
            if(reader.read()&&
                reader.node_type()==json_node_type::object) {
                
                int episode_object_depth = reader.depth();
                int season_number = -1;
                int episode_number = -1;
                while(reader.read() && 
                    reader.depth()>=episode_object_depth) {
                    // make sure we don't read any nested objects
                    // under this one
                    if(reader.depth()==episode_object_depth && 
                        reader.node_type()==json_node_type::field) {
                        if(0==strcmp("episode_number",reader.value()) && 
                            reader.read() && 
                            reader.node_type()==json_node_type::value) {
                            episode_number = reader.value_int();
                        }
                        if(0==strcmp("season_number",reader.value()) && 
                            reader.read() && 
                            reader.node_type()==json_node_type::value) {
                            season_number = reader.value_int();
                        }
                        // gather the name
                        if(0==strcmp("name",reader.value())) {
                            name[0]=0;
                            while(reader.read() && reader.is_value()) {
                                strcat(name,reader.value());
                            }
                        }
                        // gather the overview
                        if(0==strcmp("overview",reader.value())) {
                            overview[0]=0;
                            while(reader.read() && reader.is_value()) {
                                strcat(overview,reader.value());
                            }
                        }
                    }
                }
                if(season_number>-1 && episode_number>-1 && name[0]) {
                    output.printf("S%02dE%02d %s\r\n",
                        season_number,
                        episode_number,
                        name);
                    if(overview[0]) {
                        output.printf("\t%s\r\n",overview);
                    }
                    output.println("");
                }
            }
        }
    }
}
void read_series(json_reader_base& reader, Stream& output) {
    while(reader.read()) {
        // find "episodes"
        switch(reader.node_type()) {
            case json_node_type::field:
                if(0==strcmp("episodes",reader.value())) {
                    read_episodes(reader, output);
                }
                break;
            default:
                break;      
        }
    }
}

void setup() {
    Serial.begin(115200);

    if(SPIFFS.begin(false)) {
        // use binary mode in case UTF-8
        File file = SPIFFS.open("/data.json","rb");    
        if(!file) {
            return;
        }
        file_stream file_stm(file);
        json_reader file_reader(file_stm);
        // choose one or the other:
        //dump(file_reader,Serial);
        // or
        read_series(file_reader,Serial);
        file_stm.close();
    }
    constexpr static const char* wtime_url="http://worldtimeapi.org/api/ip";

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
    dump(www_reader,Serial);
    WiFi.disconnect();
}
void loop() {

}