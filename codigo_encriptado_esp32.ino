#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <mbedtls/base64.h>
#include <mbedtls/md.h>
#include <mbedtls/sha256.h>

#define LED_PIN 2  // Onboard LED is usually on GPIO 2
//Credenciales temporales
char correctUser[] = "admin";
char correctPass[] = "abc123";

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
// byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
// IPAddress ip(192, 168, 0, 117);

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
WiFiServer server(80);

// const char* ssid = "";
// const char* password = "";

int failedAttempts = 0;
unsigned long lockoutTime = 0;
String token = "";
// Clave secreta para firmar el JWT (debe ser segura en producción)
const char* secretKey = "my_super_secret_key";
unsigned long jwtExpTime = 0;
// Función para generar JWT
String createJWT(const char* user) {
  // Crear el header en formato JSON
  StaticJsonDocument<200> header;
  header["alg"] = "HS256";
  header["typ"] = "JWT";
  
  // Serializar el header a string
  String headerStr;
  serializeJson(header, headerStr);
  
  // Codificar el header en Base64
  char headerBase64[256];
  size_t headerLen;
  mbedtls_base64_encode((unsigned char*)headerBase64, sizeof(headerBase64), &headerLen, (unsigned char*)headerStr.c_str(), headerStr.length());
  headerBase64[headerLen] = '\0';

  // Crear el payload
  StaticJsonDocument<200> payload;
  payload["sub"] = user;
  payload["iat"] = millis();
  payload["exp"] = jwtExpTime;

  // Serializar el payload a string
  String payloadStr;
  serializeJson(payload, payloadStr);

  // Codificar el payload en Base64
  char payloadBase64[256];
  size_t payloadLen;
  mbedtls_base64_encode((unsigned char*)payloadBase64, sizeof(payloadBase64), &payloadLen, (unsigned char*)payloadStr.c_str(), payloadStr.length());
  payloadBase64[payloadLen] = '\0';

  // Crear la firma usando HMAC-SHA256
  String signingInput = String(headerBase64) + "." + String(payloadBase64);
  unsigned char hmacResult[32];
  mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (unsigned char*)secretKey, strlen(secretKey), (unsigned char*)signingInput.c_str(), signingInput.length(), hmacResult);

  // Codificar la firma en Base64
  char signatureBase64[256];
  size_t signatureLen;
  mbedtls_base64_encode((unsigned char*)signatureBase64, sizeof(signatureBase64), &signatureLen, hmacResult, sizeof(hmacResult));
  signatureBase64[signatureLen] = '\0';

  // Construir el JWT final
  String jwt = String(headerBase64) + "." + String(payloadBase64) + "." + String(signatureBase64);
  return jwt;
}
//Funcion de hashing
void hashPassword(const char* password, char outputBuffer[65]) {
  unsigned char hash[32];
  mbedtls_sha256_context ctx;
  
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);  // 0 para SHA-256
  mbedtls_sha256_update(&ctx, (const unsigned char*)password, strlen(password));
  mbedtls_sha256_finish(&ctx, hash);
  mbedtls_sha256_free(&ctx);

  for (int i = 0; i < 32; i++) {
      sprintf(outputBuffer + (i * 2), "%02x", hash[i]);
  }
}
//JWT Token generation
String generateToken() {
  char token[33];
  for (int i = 0; i < 32; i++) {
      token[i] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"[random(62)];
  }
  token[32] = '\0';
  return String(token);
}
//Funcion para sanitizar entradas de usuario (Solo permite caracteres alfanuméricos)
bool isValidInput(String input) {
  for (unsigned int i = 0; i < input.length(); i++) {
      if (!isalnum(input[i])) return false;
  }
  return true;
}
//Funcion para inicializar WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true); //Para conexion inestable
  WiFi.persistent(true);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado a WiFi.");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());
  server.begin(); // Inicia el servidor
}

void setup() 
{
  Serial.begin(115200);// Open serial communications and wait for port to open:
  delay(1000);
  initWiFi();
  
  pinMode(LED_PIN,OUTPUT); // Pin digital 13 como salida
}

void loop() 
{
  WiFiClient client = server.available();
  
  // Maneja las solicitudes HTTP entrantes
  String headers = "";
  String body = "";
  bool isBody = false;
  if (client) {   
    delay(100);
    if (client.connected()) {
      Serial.println("Client connected");
      while (client.available()) {
        char c = client.read();
        // Leer encabezados hasta encontrar la línea en blanco
        if (!isBody) {
            headers += c;
            if (headers.endsWith("\r\n\r\n")) {
                isBody = true; // La siguiente parte será el cuerpo
            }
        } else {
            // Leer el cuerpo del request
            body += c;
        }
      }   
      int tokenPos = headers.indexOf("token=");
      String retrivedToken = "";
      if(tokenPos != -1) {
        int initPos = 0;
        while(true){
          if (initPos == 0) {
            initPos = tokenPos + 6;
          }
          char x = headers.charAt(initPos);
          if(x=='\n'){
            break;
          }
          retrivedToken += x;
          initPos += 1;
        }
      }
      token.trim();
      retrivedToken.trim();
      // Serial.println("Headers:");
      // Serial.println(headers);
      // Serial.println("Body");
      // Serial.println(body);
      if (headers.indexOf("POST /login") != -1) {
        handleLogin(client,body);
      } else if (headers.indexOf("GET /led") != -1 && (token.length()>0&&token.equals(retrivedToken))) {
        serveResponsePage(client);
        handleLED(client,headers);
      } else if (headers.indexOf("GET /logout") != -1) {
        token = "";
        client.println("HTTP/1.1 302 Found"); 
        client.println("Location: /");  // Redirigir a inicio
        client.println("Set-Cookie: token=; path=/;"); 
      } else {
        serveLoginPage(client);
      }
    }
    delay(100); 
    client.flush(); 
    client.stop();
    Serial.println("Client disconnected");
  } 
}
// Procesar login
void handleLogin(WiFiClient client, String request) {
  if (failedAttempts >= 5 && millis() - lockoutTime < 60000) {  // 60 segundos de bloqueo
    client.println("HTTP/1.1 429 Too Many Requests");
    client.println("Retry-After: 60");
    client.println("Content-Type: text/plain");
    client.println();
    client.println("Demasiados intentos fallidos. Intente nuevamente en 60 segundos.");
    return;
  }
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

  if (!isValidInput(user) || !isValidInput(pass)) {
    Serial.println("Error: User y Password deben ser alfanumericos.");
    client.println("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nBad credentials");
    return;
  }

  if (user.equals(correctUser) && pass.equals(correctPass)) {
    jwtExpTime = millis() + 900000;
    token = createJWT(user.c_str());
    failedAttempts = 0;
    Serial.println("Login exitoso!");
    client.println("HTTP/1.1 302 Found");  // Redirección HTTP
    client.println("Location: /led");  // Redirigir a /led
    client.println("Set-Cookie: token=" + token + "; Path=/; HttpOnly; SameSite=Strict");
    client.println("Content-Type: text/html");
    client.println();
  } else {
    failedAttempts++;
    if (failedAttempts >= 5) {
      lockoutTime = millis();
    }
    Serial.println("Login fallido: usuario o contraseña incorrectos.");
    client.println("HTTP/1.1 401 Unauthorized\r\nContent-Type: text/plain\r\n\r\nInvalid credentials");
    client.println("Connection: close");
  }
}
//Controlar LED
void handleLED(WiFiClient client, String request) {
  if (request.indexOf("led_status=on") != -1) {
      digitalWrite(LED_PIN, HIGH);
      Serial.println("LED encendido");
  } else if (request.indexOf("led_status=off") != -1) {
      digitalWrite(LED_PIN, LOW);
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
  client.println("/*    General use styles     */");                                                                        
  client.println(".m-0 {margin: 0px !important;}");                                                                      
  client.println(".m-1 {margin: 1em !important;}");                                                                      
  client.println(".m-2 {margin: 2em !important;}");                                                                      
  client.println(".border-0{border-radius: 0px !important;}");                                                                       
  client.println(".float-start{float: left !important;}");                                                                       
  client.println(".float-end{float: right !important;}");                                                                        
  client.println(".glass {");                                                                        
  client.println("color: white !important;");                                                                        
  client.println("background: rgba( 255, 255, 255, 0.25 );");                                                                        
  client.println("box-shadow: 0 8px 32px 0 rgba( 155, 155, 155, 0.37 );");                                                                       
  client.println("backdrop-filter: blur( 5px );");                                                                       
  client.println("-webkit-backdrop-filter: blur( 5px );");                                                                       
  client.println("border-radius: 10px;");                                                                        
  client.println("border: 1px solid rgba( 255, 255, 255, 0.18 );");                                                                      
  client.println("}");                                                                       
  client.println(".glass a {color: white;}");                                                                        
  client.println(".white{color:white !important;}");                                                                         
  client.println("/*    Specific styles    */");                                                                     
  client.println("body {");                                                                      
  client.println("background: linear-gradient(to bottom right, #3d3d3d, black) no-repeat;");                                                                     
  client.println("height: 100vh;");                                                                      
  client.println("text-align: center;");                                                                     
  client.println("font-family: 'Trebuchet MS', sans-serif;");                                                                        
  client.println("}");                                                                       
  client.println("button {");                                                                        
  client.println("margin: 20px;");                                                                       
  client.println("padding: 15px;");                                                                      
  client.println("font-size: 20px;");                                                                        
  client.println("color: white;");                                                                       
  client.println("border: none;");                                                                       
  client.println("border-radius: 5px;");                                                                         
  client.println("}");                                                                       
  client.println("#btn-bg-color {");                                                                         
  client.println("position: absolute;");                                                                         
  client.println("background-color: white;");                                                                        
  client.println("width: 80px;");                                                                        
  client.println("height: 80px;");                                                                       
  client.println("border-radius: 50px;");                                                                        
  client.println("left: 1%;");                                                                       
  client.println("top: 10%;");                                                                       
  client.println("filter: blur(15px);");                                                                     
  client.println("z-index: -5;");                                                                        
  client.println("transition: width 0.5s ease-in-out, height 0.5s ease-in-out, transform 0.5s ease-in-out;");                                                                        
  client.println("}");                                                                           
  client.println("/* Expande en dirección normal al hacer hover */");                                                                        
  client.println("#btnControl:hover ~ #btn-bg-color {");                                                                     
  client.println("transform: translate(-5px,-10px);");                                                                       
  client.println("width: 302px;");                                                                       
  client.println("height: 100px;");                                                                      
  client.println("}");                                                                       
  client.println("/* Se mueve a la derecha cuando está marcado */");                                                                         
  client.println("#btnControl:checked ~ #btn-bg-color {");                                                                       
  client.println("transform: translateX(215px);");                                                                       
  client.println("}");                                                                           
  client.println("/* Cuando está marcado y el mouse pasa por encima, crece en dirección opuesta */");                                                                        
  client.println("#btnControl:checked:hover ~ #btn-bg-color {");                                                                         
  client.println("width: 302px;");                                                                       
  client.println("height: 100px;");                                                                      
  client.println("transform: translate(-5px, -10px); /* Se mueve menos para expandirse a la izquierda */");                                                                      
  client.println("}");                                                                       
  client.println("#btn-bg{");                                                                        
  client.println("background: rgba( 255, 255, 255, 0.25 );");                                                                        
  client.println("box-shadow: 0 5px 10px 0 grey;");                                                                      
  client.println("backdrop-filter: blur( 5px );");                                                                       
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
  client.println("#btnControl {");                                                                       
  client.println("display: none;");                                                                      
  client.println("}");                                                                       
  client.println("label.btn {");                                                                     
  client.println("display: block;  /* Hace que el label se comporte como un bloque */");                                                                         
  client.println("width: 100%;  /* Ocupa todo el ancho de #btn-bg */");                                                                      
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
  client.println("#switch-button{");                                                                         
  client.println("margin-left: -12.5em;");                                                                       
  client.println("margin-right: auto;");                                                                     
  client.println("}");                                                                       
  client.println("/*   navbar styles   */");                                                                     
  client.println("ul {");                                                                        
  client.println("width: 100%;");                                                                        
  client.println("height: 3em;");                                                                        
  client.println("list-style-type: none;");                                                                      
  client.println("margin: 0;");                                                                      
  client.println("padding: 0;");                                                                     
  client.println("overflow: hidden;");                                                                       
  client.println("}");                                                                           
  client.println("li {float: left;}");                                                                       
  client.println("li a {");                                                                      
  client.println("display: block;");                                                                         
  client.println("text-align: center;");                                                                         
  client.println("padding: 14px 16px;");                                                                         
  client.println("text-decoration: none;");                                                                      
  client.println("}");                                                                       
  client.println("li a:hover:not(.active) {");                                                                       
  client.println("background-color: grey;");                                                                         
  client.println("}");                                                                       
  client.println(".active {");                                                                       
  client.println("color: black !important;");                                                                        
  client.println("background-color: #FFFF;");                                                                        
  client.println("}");                                                                       
  client.println("/*    dropdown btn    */");                                                                        
  client.println(".dropbtn {");                                                                      
  client.println("padding: 16px;");                                                                      
  client.println("font-size: 16px;");                                                                        
  client.println("border: none;");                                                                       
  client.println("cursor: pointer;");                                                                        
  client.println("}");                                                                       
  client.println(".dropdown {");                                                                         
  client.println("display: inline-block;");                                                                      
  client.println("overflow: hidden;");                                                                       
  client.println("}");                                                                       
  client.println(".dropdown-content {");                                                                     
  client.println("display: none;");                                                                      
  client.println("position: absolute;");                                                                         
  client.println("min-width: 160px;");                                                                       
  client.println("right: 0;");                                                                       
  client.println("box-shadow: 0px 8px 16px 0px rgba(255,255,255,0.2);");                                                                       
  client.println("z-index: 0;");                                                                     
  client.println("}");                                                                       
  client.println(".dropdown-content a {");                                                                       
  client.println("padding: 12px 16px;");                                                                         
  client.println("text-decoration: none;");                                                                      
  client.println("display: block;");                                                                     
  client.println("}");                                                                       
  client.println(".dropdown:hover .dropdown-content {");                                                                         
  client.println("display: block;");                                                                     
  client.println("}");                                                                       
  client.println("</style>");                                                                        
  client.println("</head>");                                                                         
  client.println("<body class='m-0'>");                                                                      
  client.println("<header style='width:100%;'>");                                                                        
  client.println("<nav class='glass border-0'>");                                                                        
  client.println("<ul>");                                                                        
  client.println("<li>");                                                                        
  client.println("<a class='active' href='#home'>Inicio</a>");                                                                       
  client.println("</li>");                                                                       
  client.println("<li class='dropdown float-end'>");                                                                     
  client.println("<a class='dropbtn m-0 border-0'>Opciones</a>");                                                                        
  client.println("<div class='dropdown-content'>");                                                                      
  client.println("<a href='/change_password'>Cambiar contrase&ntildea</a>");                                                                     
  client.println("<a href='/logout'>Cerrar sesion</a>");                                                                         
  client.println("</div>");                                                                      
  client.println("</li>");                                                                           
  client.println("</ul>");                                                                       
  client.println("</nav>");                                                                      
  client.println("</header>");                                                                       
  client.println("<h1 class='white'>Control LED via Web Server</h1>");                                                                     
  client.println("<div class='white m-1'><b>Estado del LED: <p id='statusText' style='display: inline;'></p></b></div>");
  client.println("<div id='btn-bg'>");                                                                           
  client.println("<form id='led-btn' onsubmit='return test(this)'>");                                                                            
  client.println("<input type='checkbox' id='btnControl'/>");                                                                            
  client.println("<label class='btn' for='btnControl'>");                                                                        
  client.println("<svg id='switch-button' xmlns='http://www.w3.org/2000/svg' version='1.0' width='100px' height='100px' viewBox='0 0 100.000000 100.000000' preserveAspectRatio='xMidYMid meet'>");
  client.println("<g transform='translate(5, 88) scale(0.025,-0.025)' stroke='none'>");                                                                     
  client.println("<path d='M1295 2984 c-346 -53 -628 -197 -867 -442 -325 -335 -478 -794 -412 -1241 49 -336 180 -604 412 -843 524 -539 1330 -609 1951 -170 171 121 360 342 457 535 192 381 215 818 63 1212 -153 395 -470 712 -868 866 -224 87 -505 119 -736 83z m294 -412 c14 -10 35 -34 46 -53 18 -31 20 -55 23 -269 2 -129 0 -263 -3 -297 -11 -104 -70 -165 -160 -165 -58 0 -104 27 -132 78 -22 38 -23 47 -23 329 0 264 2 293 19 325 45 84 154 109 230 52z m-579 -262 c20 -5 51 -26 70 -45 29 -30 34 -43 38 -94 5 -68 -16 -114 -68 -151 -41 -29 -123 -135 -159 -207 -96 -189 -94 -422 6 -623 55 -110 179 -231 293 -288 105 -51 198 -73 310 -73 112 0 205 22 310 73 121 60 239 178 297 298 97 199 97 422 3 611 -34 68 -74 122 -151 205 -54 58 -64 85 -56 150 10 88 105 150 191 124 75 -23 227 -193 299 -335 47 -94 84 -226 97 -341 31 -302 -70 -591 -285 -808 -88 -89 -167 -144 -289 -202 -149 -70 -234 -88 -416 -88 -182 0 -267 18 -416 88 -122 58 -201 113 -289 202 -164 166 -261 371 -286 603 -32 307 77 600 301 815 93 89 132 105 200 86z'/>");                                                                      
  client.println("</g>");                                                                        
  client.println("</svg>");                                                                      
  client.println("</label>");                                                                        
  client.println("<div id='btn-bg-color'></div>");                                                                       
  client.println("</form>");                                                                     
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
  client.println("let estado = this.checked ? 'on' : 'off'; // Determina el estado del led_status");                                                                     
  client.println("// Enviar la solicitud usando fetch");                                                                         
  client.println("fetch('/led?led_status=' + estado, { method: 'GET' })");                                                                       
  client.println(".then(response => response.text())");                                                                          
  client.println(".catch(error => console.error('Error:', error));");                                                                       
  client.println("});");                                                                      
  client.println("</script>");                                                                       
  client.println("</body>");                                                                         
  client.println("</html>");                                                                     
}