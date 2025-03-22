#include <SPI.h>
#include <WiFi.h>

#define LED_PIN 2  // Onboard LED is usually on GPIO 2

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
// byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
// IPAddress ip(192, 168, 0, 117);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
WiFiServer server(80);

// const char* ssid = "LATINA_OPEN";
// const char* password = "";
const char* ssid = ".TigoWiFi-307776204/0";
const char* password = "WiFi-94330935";

//Funcion para inicializar WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true); //Para conexion inestable
  WiFi.persistent(true); // se intenta reconectar
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConectado a WiFi.");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());

  server.begin(); // Inicia el servidor
  //server.setTimeout(2000);
}

void generateHexValues(int length, char output[][5]) {
    for (int i = 0; i < length; i++) {
        byte randomValue = random(0, 256); // Genera un valor entre 0x00 y 0xFF
        sprintf(output[i], "0x%02X", randomValue); // Formatea en hexadecimal
    }
}

char correctUser[] = "admin";
char correctPass[] = "abc123"; // La contraseña real en texto plano (debe cifrarse)

bool session_active = false;

char header[500];
int bufferSize = 0;
int LED = 2; // Pin digital para el LED
String estado = "OFF"; // Estado del LED inicialmente "OFF"

void setup() 
{
  Serial.begin(115200);// Open serial communications and wait for port to open:
  // start the Ethernet connection and the server:
  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  delay(1000);
  initWiFi();

  pinMode(LED_PIN,OUTPUT); // Pin digital 13 como salida
}

void loop() 
{
  WiFiClient client = server.available();
    // Maneja las solicitudes HTTP entrantes

  if (client) {
      Serial.println("New client connected");
      // an http request ends with a blank line
      
      String request = readRequest(client);
      
      if (request.indexOf("POST /login") != -1) {
        handleLogin(client);
      } else if (session_active) {
        serveResponsePage(client);
        handleLED(client);
      } else {
        serveLoginPage(client);
      }

      delay(500); 
      client.flush(); 
      client.stop();
      Serial.println("Client disconnected");
  }
}//void loop

// Leer la solicitud HTTP del cliente
String readRequest(WiFiClient client) {
    String request = "";
    unsigned long timeout = millis() + 2000; // Espera máximo 1 segundo

    while (client.available() && millis() < timeout) {
        char c = client.read();
        request += c;
        timeout = millis() + 100;  // Extender tiempo mientras haya datos
        if (c == '\n' && request.endsWith("\r\n\r\n")) {
            break;  // Fin de la solicitud HTTP
        }
    }
    return request;
}

// Procesar login
void handleLogin(WiFiClient client) {
  String request = readRequest(client);

  int userPos = request.indexOf("user=");
  int passPos = request.indexOf("pass=");

  if (userPos == -1 || passPos == -1) {
      Serial.println("Error: No se encontraron parámetros 'user' y 'pass'.");
      client.println("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nMissing credentials");
      return;
  }

  int userEnd = request.indexOf("&", userPos);
  int passEnd = request.indexOf(" ", passPos);

  if (userEnd == -1) userEnd = passPos - 1;
  if (passEnd == -1) passEnd = request.length();

  String user = request.substring(userPos + 5, userEnd);
  String pass = request.substring(passPos + 5, passEnd);

  user.trim();
  pass.trim();

  Serial.print("Usuario recibido: ");
  Serial.println(user);
  Serial.print("Contraseña recibida: ");
  Serial.println(pass);

  if (user.equals(correctUser) && pass.equals(correctPass)) {
      session_active = true;
      Serial.println("Login exitoso!");
      client.println("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
  } else {
      Serial.println("Login fallido: usuario o contraseña incorrectos.");
      client.println("HTTP/1.1 401 Unauthorized\r\nContent-Type: text/plain\r\n\r\nInvalid credentials");
  }
}
//Controlar LED
void handleLED(WiFiClient client) {
  String request = readRequest(client);

  if (request.indexOf("led_status=on") != -1) {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED encendido");
      estado = "ON";
  } else if (request.indexOf("led_status=off") != -1) {
      digitalWrite(LED_PIN, LOW);
      estado = "OFF";
      Serial.println("LED apagado");
  }
}
// Servir la página de login
void serveLoginPage(WiFiClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();
  client.println("<!DOCTYPE html>");
  client.println("<html>");
  client.println("<head>");
  client.println("<meta charset='UTF-8'>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
  client.println("<title>login</title>");
  client.println("<style>");
  client.println("body {");
  client.println("background: linear-gradient(to bottom right, #3d3d3d, black) no-repeat;");
  client.println("height: 100vh;");
  client.println("font-family: 'Trebuchet MS', sans-serif;");
  client.println("display: flex;");
  client.println("justify-content: center;");
  client.println("align-items: center;");
  client.println("}");
  client.println("#login-container-bg {");
  client.println("position: absolute;");
  client.println("background-color: white;");
  client.println("width:  20em;");
  client.println("height: 5em;");
  client.println("border-radius: 50px;");
  client.println("left: 50%;");
  client.println("top: 50%;");
  client.println("/* Add the blur effect */");   
  client.println("filter: blur(15px);"); 
  client.println("-webkit-filter: blur(15px);"); 
  client.println("transform: translate(-50%, -50%);");   
  client.println("z-index: 0;"); 
  client.println("transition: width 1s, height 1s, transform 1s;");
  client.println("}");
  client.println("#login-container {");
  client.println("background: rgba( 255, 255, 255, 0.25 );");
  client.println("box-shadow: 0 5px 10px 0 grey;");
  client.println("backdrop-filter: blur( 5px );");
  client.println("-webkit-backdrop-filter: blur( 5px );");
  client.println("z-index: 1;");
  client.println("width: 15em;");
  client.println("padding: 20px;"); 
  client.println("border-radius: 10px;");
  client.println("transition: box-shadow 0.25s ease-in-out;");
  client.println("}");
  client.println("#login-container:hover + #login-container-bg {");  
  client.println("width: 20em;");
  client.println("height: 18em;");
  client.println("border-radius: 15px;");
  client.println("transform: translate(-50%, -50%) translateY(5px);");
  client.println("}");
  client.println("input {");
  client.println("display: block;");
  client.println("width: 15em;");
  client.println("margin-bottom: 10px;");
  client.println("padding: 8px;");
  client.println("border-radius: 5px;");
  client.println("border: 1.5px solid grey;");
  client.println("}");
  client.println("button {");
  client.println("width: 5em;");
  client.println("padding: 8px;");
  client.println("background: linear-gradient(to top right, #343a40 50%, #212529 50%);");
  client.println("background-size: 200% 200%;");
  client.println("background-position: top right;");
  client.println("color: white;");  
  client.println("border: none;"); 
  client.println("cursor: pointer;");
  client.println("border-radius: 5px;");
  client.println("transition: background-position 0.5s ease-in-out;");
  client.println("}");
  client.println("button:hover {");
  client.println("background-position: bottom left;");
  client.println("}");
  client.println("</style>");
  client.println("</head>");
  client.println("<body> ");
  client.println("<div id='login-container'>");
  client.println("<h2 style='color:#212529;'>Login</h2>");
  client.println("<form method='POST' action='/login'>");
  client.println("<input type='text' name='user' placeholder='Usuario' required>");
  client.println("<input type='password' name='pass' placeholder='Contraseña' required>");
  client.println("<button type='submit'>Ingresar</button>");
  client.println("</form>");
  client.println("</div>");
  client.println("<div id='login-container-bg'></div>");
  client.println("</body>");
  client.println("</html>");
}

// Página de respuesta
void serveResponsePage(WiFiClient client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();
  client.println("<!DOCTYPE html>");
  client.println("<html>");
  client.println("<head>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0' charset='utf-8'>");
  client.println("<style>");
  client.println("body { ");
  client.println("background: linear-gradient(to bottom right, #3d3d3d, black) no-repeat;");
  client.println("height: 100vh;");
  client.println("text-align: center; ");
  client.println("font-family: 'Trebuchet MS', sans-serif;");
  client.println("}");
  client.println(".white{");
  client.println(" color:white;");
  client.println("}");
  client.println("button { ");
  client.println(" margin: 20px; ");
  client.println(" padding: 15px; ");
  client.println(" font-size: 20px; ");
  client.println(" color: white; ");
  client.println(" border: none; ");
  client.println(" border-radius: 5px; ");
  client.println("}");
  client.println("#btn-bg-color{");
  client.println(" position: absolute;");
  client.println(" background-color: white;");
  client.println(" width: 300px;");
  client.println(" height: 100px;");
  client.println(" border-radius: 50px;");
  client.println(" margin-left: auto;");
  client.println(" margin-right: auto;");
  client.println(" z-index: -1;");
  client.println(" transform: translateY(10px);");
  client.println("}");
  client.println("#btn-bg{ "); 
  client.println("background: rgba( 255, 255, 255, 0.25); ");
  client.println("box-shadow: 0 5px 10px 0 grey;");
  client.println("backdrop-filter: blur( 5px);");
  client.println("-webkit-backdrop-filter: blur( 5px );");
  client.println("border-radius: 10px;");
  client.println("border: 1px solid rgba( 255, 255, 255, 0.18 );");
  client.println("width: 300px;");
  client.println("height: 100px;");
  client.println("border-radius: 50px;");
  client.println("margin-left: auto;");
  client.println("margin-right: auto;");
  client.println("z-index: 1;");
  client.println("transition: box-shadow 0.25s ease-in-out;");
  client.println("}");
  client.println("#btn-bg:hover {");
  client.println(" box-shadow: 0 5px 20px 0 grey;");
  client.println(" }");
  client.println("#btnControl {");
  client.println(" display: none;");
  client.println("}");
  client.println("label.btn {");
  client.println("display: block;/* Hace que el label se comporte como un bloque */");
  client.println("width: 100%;/* Ocupa todo el ancho de #btn-bg */");
  client.println("height: 100%; /* Ocupa todo el alto de #btn-bg */");
  client.println("cursor: pointer; /* Asegura que se muestre el cursor de clic */");
  client.println("top: 0;");
  client.println("left: 0;");
  client.println("}");
  client.println("#btnControl + label > svg {");
  client.println("z-index: 2;");
  client.println("fill:red;");
  client.println("opacity: 0.5;");
  client.println("transition: transform 0.5s ease-in-out, fill 0.25s ease-in-out;");
  client.println("}");
  client.println("#btnControl:checked + label > svg {");
  client.println("z-index: 2;");
  client.println("fill:green;");
  client.println("opacity: 0.5;");
  client.println("transform: translateX(215px);");
  client.println("}");
  client.println("#statusText {");
  client.println("opacity: 0;");
  client.println("transition: opacity 0.5s ease-in-out;");
  client.println("}");
  client.println("</style>");
  client.println("</head>");
  client.println("<body>");
  client.println("<h1 class='white'>Control LED via Web Server</h1>");
  client.println("<div class='white' style='margin-bottom: 10px;'><b>Estado del LED: <p id='statusText' style='display: inline;'></p></b></div>");
  client.println("<div id='btn-bg'>");
  client.println(" <div id='btn-bg-color'></div>");
  client.println(" <form id='led-btn' method='GET' action='/led'>");
  client.println(" <input type='checkbox' id='btnControl' name='led_status'/>");
  client.println(" <label class='btn' for='btnControl'>");
  client.println(" <svg id='switch-button' xmlns='http://www.w3.org/2000/svg' version='1.0' width='300px' height='100px' viewBox='0 0 300.000000 100.000000' preserveAspectRatio='xMidYMid meet'>");
  client.println(" <g transform='translate(5, 88) scale(0.025,-0.025)' stroke='none'>");
  client.println(" <path d='M1295 2984 c-346 -53 -628 -197 -867 -442 -325 -335 -478 -794 -412 -1241 49 -336 180 -604 412 -843 524 -539 1330 -609 1951 -170 171 121 360 342 457 535 192 381 215 818 63 1212 -153 395 -470 712 -868 866 -224 87 -505 119 -736 83z m294 -412 c14 -10 35 -34 46 -53 18 -31 20 -55 23 -269 2 -129 0 -263 -3 -297 -11 -104 -70 -165 -160 -165 -58 0 -104 27 -132 78 -22 38 -23 47 -23 329 0 264 2 293 19 325 45 84 154 109 230 52z m-579 -262 c20 -5 51 -26 70 -45 29 -30 34 -43 38 -94 5 -68 -16 -114 -68 -151 -41 -29 -123 -135 -159 -207 -96 -189 -94 -422 6 -623 55 -110 179 -231 293 -288 105 -51 198 -73 310 -73 112 0 205 22 310 73 121 60 239 178 297 298 97 199 97 422 3 611 -34 68 -74 122 -151 205 -54 58 -64 85 -56 150 10 88 105 150 191 124 75 -23 227 -193 299 -335 47 -94 84 -226 97 -341 31 -302 -70 -591 -285 -808 -88 -89 -167 -144 -289 -202 -149 -70 -234 -88 -416 -88 -182 0 -267 18 -416 88 -122 58 -201 113 -289 202 -164 166 -261 371 -286 603 -32 307 77 600 301 815 93 89 132 105 200 86z'/>");
  client.println(" </g>");
  client.println(" </svg>");
  client.println(" </label>");
  client.println(" </form>");
  client.println("</div>");
  client.println("<script>"); 
  client.println("document.getElementById('btnControl').addEventListener('change', function() {");
  client.println("event.preventDefault();");
  client.println("let statusText = document.getElementById('statusText');");  
  client.println("if (this.checked) {");  
  client.println("statusText.textContent = 'Encendido';"); 
  client.println("statusText.style.color = 'green';");
  client.println("} else {"); 
  client.println("statusText.textContent = 'Apagado';");  
  client.println("statusText.style.color = 'red';");  
  client.println("}");
  client.println("statusText.style.opacity = '1';");  
  client.println("let estado = this.checked ? 'on' : 'off'; // Determina el estado del LED");
  client.println("// Enviar la solicitud usando fetch");  
  client.println("fetch('/led?led_status=' + estado, { method: 'GET' })");
  client.println(".then(response => response.text())");   
  client.println(".then(data => {");  
  client.println("console.log('Respuesta del servidor: ' + data); // Ver la respuesta en consola");   
  client.println("statusText.textContent = estado.toUpperCase(); // Actualizar estado en pantalla");  
  client.println("})");   
  client.println(".catch(error => console.error('Error:', error));"); 
  client.println("});");  
  client.println("</script>");
  client.println("</body>");
  client.println("</html>");
}

