#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <WebSocketsServer.h>

#ifndef STASSID
#define STASSID "MY_NETWORK"
#define STAPSK  "MY_PASSWORD"
#endif

#define IMAGE_WIDTH 384

const char* ssid = STASSID;
const char* password = STAPSK;

ESP8266WebServer server(80);

const int led = 13;

bool socket_is_open = false;
uint8_t client_num;

uint8_t print_fsm = 0;

const char* print_command = "print_img ";

String data_for_printer;
uint8_t tempx;
String tempval;

int bytes_so_far;

WebSocketsServer webSocket = WebSocketsServer(81);

const char *page_index = R"=====(
<!DOCTYPE html>
<html>
  <head>
    <title>QR701</title>
    <style>
      body {
        background: #9a9abf;
      }
    </style>
    <script src='zuri.js'></script>
  </head>
  <body onload="my_setup()">
    <input type="file" id="myFile">Load image</input>
    <br>
    <img id="myImage" style="border:black 1px" />
    <img id="myImage2" style="border:black 2px" />
    <dir id='container'></dir>
    <input id='prnt_btn' type='button' onclick='func_print()' value='print'></input>
    <input type='text' width='60' id='my_input' />
    <button type='button' onclick='traducir()'>Translate</button>
  </body>
</html>
)=====";

const char *js_code = R"=====(
    const IMAGE_WIDTH = 384;
    var canvas = document.createElement("canvas");
    var ctx = canvas.getContext("2d");
    var imageData;

    var offset_img = 24;

    var final_d = [];
    
    var condensed_data = [];

    var server_ready = false;
    var send_data = false;
    var data_index;

    var xxx;

    var inv_img = false;

    var padded_height;
  
    //window.onload = function() {
    //}

    function send_chunck() {
      var data_str = "DATA ";
      if(data_index >= final_d.length) {
        console.log("no more data");
        Socket.send("END");
        document.getElementById('prnt_btn').disabled = false;
      } else {
        for(xxx= 0; (xxx< 16) && (data_index < final_d.length); ++xxx) {
          data_str += final_d[data_index++] + " ";
        }
        Socket.send(data_str);
      }
    }

  function my_setup() {
    Socket = new WebSocket('ws://' + window.location.hostname + ':81/');
    Socket.onmessage = function(event) {
      console.log(event.data);
      server_ready = true;
      if(event.data=="YES!") {
        //console.log("start sending data");
        send_data = true;
        data_index = 0;
        send_chunck();      
      } else {
        if(event.data = "RTR!") {
          send_chunck(); 
        } else {
          console.log(event.data);
        }
      }
    }
    Socket.onopen = function() {
      Socket.send('.');
      console.log('here');
      server_ready = true;
    }
    Socket.onclose = function() {
      console.log('died');
      server_ready = false;
      send_data = false;
    }

    document.getElementById('myFile').onchange = function (evt) {
      var tgt = evt.target || window.event.srcElement,
      files = tgt.files;
      
      // FileReader support
      if (FileReader && files && files.length) {
        var fr = new FileReader();
        fr.readAsDataURL(files[0]);
        fr.onload = () => showImage(fr);
        inv_img = false;
      }
    }

    document.getElementById('container').appendChild(canvas);
    canvas.setAttribute('style', 'border: 2px solid blue');
    canvas.width = IMAGE_WIDTH;
    canvas.height = 24;
  }

  function showImage(fileReader) {
        var img = document.getElementById("myImage");
        img.src = fileReader.result;
        img.onload = () => getImageData(img);
      }

      function httpGet(theUrl) {
        let xmlHttpReq = new XMLHttpRequest();
        xmlHttpReq.open("GET", theUrl, false); 
        xmlHttpReq.send(null);
        return xmlHttpReq.responseText;
      }

      function getImageData(img) {
        //document.getElementById('container').appendChild(canvas);
        
        ratio = img.height / img.width;
        new_height = Math.round(IMAGE_WIDTH * ratio)
        
        
        padded_height = Math.ceil(new_height / 24) * 24;
        
        canvas.width = IMAGE_WIDTH;
        canvas.height = new_height;
        
        ctx.drawImage(img, 0, 0, IMAGE_WIDTH, new_height);
        imageData = ctx.getImageData(0, 0, IMAGE_WIDTH, new_height);
        var i;
        for (i = 0; i < imageData.data.length; i += 4) {
          imageData.data[i+0] ^= 0xff;
          imageData.data[i+1] ^= 0xff;
          imageData.data[i+2] ^= 0xff;
          imageData.data[i+3] = 0xff;
        }
        
        // now make padded image, pad with red
        canvas.height = padded_height;
        ctx.fillStyle = 'red';
        ctx.fillRect(0, 0, IMAGE_WIDTH, padded_height);       
                
        ctx.putImageData(imageData, 0, 0);
        
        console.log(IMAGE_WIDTH + " " + padded_height);
        
        // dump new padded data
        //imageData = ctx.getImageData(0, 0, 168, padded_height);
        //console.log(imageData.data);
      }
      
      function re_format() {
        final_d = [];
        var i = 0;
        
        for(x= 0; x< condensed_data.length; x += 8) {
          mask = 0;
          if(condensed_data[x + 0]) mask |= 0x80;
          if(condensed_data[x + 1]) mask |= 0x40;
          if(condensed_data[x + 2]) mask |= 0x20;
          if(condensed_data[x + 3]) mask |= 0x10;
          if(condensed_data[x + 4]) mask |= 0x08;
          if(condensed_data[x + 5]) mask |= 0x04;
          if(condensed_data[x + 6]) mask |= 0x02;
          if(condensed_data[x + 7]) mask |= 0x01;
          
          final_d[i] = mask;
          ++i;
        }
        
        //console.log(final_d);
        if(server_ready) {
          Socket.send('print_img ' + padded_height);
        }
      }
      
      function process_24x24(img_d) {
        var d = [];
        var x1, y1;
        var s1, d2;
        s1 = 0;
        d2 = 0;
        //condensed_data = [];
        //index = 0;
        for(x1 = 0; x1< 24; ++x1) {
          d2 = x1 * 4;
          for(y1 = 0; y1 < 24; ++y1) {
            d[d2 + 0] = img_d.data[s1 + 0];
            d[d2 + 1] = img_d.data[s1 + 1];
            d[d2 + 2] = img_d.data[s1 + 2];
            d[d2 + 3] = img_d.data[s1 + 3];
           
            // condensed data
            var val = 0x00;
            if(img_d.data[d2 + 1] > 127) val = 0xff;
            if(inv_img) val ^= 0xff;  // flip shopping list
            condensed_data.push(val);
           
            s1 += 4;
            d2 += (24 * 4);
           
          }
        }
        for(x1 = 0; x1 <= d.length; ++x1) img_d.data[x1] = d[x1];
        return img_d;
      }

      function process_24(img_d) {
        var x1;
        var val;
        for(x1 = 0; x1 < img_d.data.length; x1 += 4) {
          val = 0;
          if(img_d.data[x1 + 1] > 127) val = 0xff; 
          condensed_data.unshift(val);
        }
      }
      
      function func_print() {   
        // reset data
        //var x1;
        //var val;
        document.getElementById('prnt_btn').disabled = true;
        final_d = [];
        condensed_data = [];
        // break into blocks 24
        var x, y;
        for(y=0; y< canvas.height; y += 24) {
          for(x=0; x< IMAGE_WIDTH; x += 24) {
            // get section
            segment = ctx.getImageData(x, y, 24, 24);
            segment = process_24x24(segment);
            //process_24(segment);
            //ctx.putImageData(segment, x, y);
            //ctx.fillStyle = 'blue';
            //ctx.fillRect(x, y, 8, 8);
          }
        }

       //for(x1 = 0; x1 < imageData.data.length; x1 += 4) {
        //  val = 0;
        //  if(imageData.data[x1 + 1] > 127) val = 0xff; 
       //   condensed_data.unshift(val);
       //}
        
        //console.log(condensed_data);
        re_format();
      }

      function traducir() {
        inv_img = true;
        data = document.getElementById('my_input').value;
        item_en = data;
        my_word = "";
        
        try {
          data = httpGet("https://translate.googleapis.com/translate_a/single?client=gtx&sl=auto&tl=ru&dt=t&q=" + data);
          const obj = JSON.parse(data);
          my_word = obj[0][0][0];
        } catch(e) {
          my_word = "????";
        }
        
        //alert(data);
        console.log(item_en + " - " + my_word);
        // add to list
        
        if(canvas.height < offset_img) {
          segment = ctx.getImageData(0, 0, IMAGE_WIDTH, offset_img);
          canvas.height += 24;
          ctx.putImageData(segment, 0, 0);
        }
        
        ctx.fillStyle = 'white';
        ctx.fillRect(0, offset_img - 24, IMAGE_WIDTH, 24);
        ctx.fillStyle = 'black';
        ctx.font = "22px Tahoma";
        //ctx.fillText(item_en + " - " + my_word, 0, offset_img - 5);
        ctx.textAlign = "end";
        ctx.fillText(my_word, IMAGE_WIDTH, offset_img - 5);
        ctx.textAlign = "start";
        ctx.fillText(item_en, 0, offset_img - 5);
        offset_img += 24;
        
        
        document.getElementById('my_input').value = "";       
      }
      
)=====";

void handleRoot() {
  server.send(200, "text/html", page_index);
}

void handleJS() {
  server.send(200, "text/script", js_code);
}

void handleFavicon() {
  static const uint8_t gif[] PROGMEM = {
      0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x10, 0x00, 0x10, 0x00, 0x80, 0x01,
      0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x2c, 0x00, 0x00, 0x00, 0x00,
      0x10, 0x00, 0x10, 0x00, 0x00, 0x02, 0x19, 0x8c, 0x8f, 0xa9, 0xcb, 0x9d,
      0x00, 0x5f, 0x74, 0xb4, 0x56, 0xb0, 0xb0, 0xd2, 0xf2, 0x35, 0x1e, 0x4c,
      0x0c, 0x24, 0x5a, 0xe6, 0x89, 0xa6, 0x4d, 0x01, 0x00, 0x3b
    };
    char gif_colored[sizeof(gif)];
    memcpy_P(gif_colored, gif, sizeof(gif));
    // Set the background to a random set of colors
    gif_colored[16] = random(0, 255);//millis() % 256;
    gif_colored[17] = random(0, 255);//millis() % 256;
    gif_colored[18] = random(0, 255);//millis() % 256;
    server.send(200, "image/gif", gif_colored, sizeof(gif_colored));
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    String str = (char *) payload;
    int value;

    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            socket_is_open = false;
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                //Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
                client_num = num;
                socket_is_open = true;
            }
            break;
        case WStype_TEXT:
            //Serial.printf("[%u] get Text: %s\n", num, payload);

            // "print_img "
            if(str.length() > 11) {
              //if(strcmp((char *)str.substring(0, 9), print_command) == 0) {
              if(str.substring(0, 10) == print_command) {
                //Serial.println("PRINT!");
                value = str.substring(10).toInt();
                //Serial.printf("image has height of %d\n", value);
                // request data
                print_fsm = 1;
              } else {
                if(str.substring(0, 5) == "DATA ") {
                  //Serial.print(str);
                  //Serial.println('#');
                  //webSocket.sendTXT(num, "RTR!");
                  data_for_printer = str.substring(5);
                  print_fsm = 2;
                }
              }
            } else {
              if(str.substring(0, 3) == "END") {
                print_fsm = 3;
              }
            }

            // send message to client
            //webSocket.sendTXT(num, "1234");
            //client_num = num;

            //doSomething = true;

            // send data to all connected clients
            // webSocket.broadcastTXT("message here");
            break;
        case WStype_BIN:
            //Serial.printf("[%u] get binary length: %u\n", num, length);
            //hexdump(payload, length);

            // send message to client
            // webSocket.sendBIN(num, payload, length);
            break;
    }

}

void handleNotFound() {
  //digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  //digitalWrite(led, 0);
}

void setup(void) {
  print_fsm = 0;
  //printer.println("connection");
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  //Serial.println("");

  socket_is_open = false;

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Serial.print(".");
  }
  //Serial.println("");
  //Serial.print("Connected to ");
  //Serial.println(ssid);
  //Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("");
  Serial.println("");
  Serial.println("");

  //if (MDNS.begin("esp8266")) {
   // Serial.println("MDNS responder started");
  //}

  server.on("/", handleRoot);
  server.on("/zuri.js", handleJS);
  server.on("/favicon.ico", handleFavicon);

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  

  server.onNotFound(handleNotFound);
  server.begin();
  //Serial.println("HTTP server started");
}

void loop(void) {
  webSocket.loop();
  server.handleClient();
  MDNS.update();

  if(print_fsm == 1) {
    //Serial.println("init printer!");
    //Serial.write();
    webSocket.sendTXT(client_num, "YES!");
    print_fsm = 0;
    bytes_so_far = 0;
  }
  if(print_fsm == 2) {
    //Serial.println(data_for_printer);
    if(bytes_so_far == 0) {
      //Serial.println("open header");
      Serial.write(byte(27));
      Serial.write(byte(42));
      Serial.write(byte(33));
      Serial.write(byte(IMAGE_WIDTH & 0xff));
      Serial.write(byte(IMAGE_WIDTH >> 8));
    }
    
    data_for_printer.trim();
    tempval = "";
    for(tempx=0; tempx < data_for_printer.length(); ++tempx) {
      if(data_for_printer[tempx] != ' ') tempval += data_for_printer[tempx];
      else break;      
    }
    data_for_printer = data_for_printer.substring(tempx);
    data_for_printer.trim();
    Serial.write(tempval.toInt());
    //Serial.printf("%02X ", tempval.toInt());
    ++bytes_so_far;
    if(bytes_so_far >= 1152) {
      bytes_so_far = 0;
      //Serial.println(" close header");
      Serial.write(byte(29));
      Serial.write(byte(47));
      Serial.write(byte(1));
    }
    //Serial.print(' ');

    if(data_for_printer.length() == 0) {
      //Serial.println("");
      webSocket.sendTXT(client_num, "RTR!");
      print_fsm = 0;
    }
  }
  if(print_fsm == 3) {
    //Serial.println("close printer");
    print_fsm = 0;
  }
}

/*
 * for(int y=0; y<h; y += 8) {
 
    writeBytes(27);
    writeBytes(42);
    writeBytes(0);
    writeBytes(w);
    writeBytes(0);
    
    for(int i = 0; i < w; ++i) {
      //bout = 0;
      //if(i > (w/2)) bout = 0x42;
      //writeBytes(bout);
      bout = pgm_read_byte(bitmap + data);
      bout ^= 0xff;
      timeoutWait();
      //writeBytes(bout);
      stream->write(bout);
      ++data;
    }
    
    writeBytes(29);
    writeBytes(47);
    writeBytes(1);
  }
 * 
 * 
 */
