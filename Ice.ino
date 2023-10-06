import network
import machine
import time
from machine import Pin, PWM
from microWebSrv import MicroWebSrv

# Configuration Wi-Fi
ssid = "ICE_OS"
password = "123456789"

# Configuration des broches GPIO
LED_BL2 = Pin(15, Pin.OUT)  # Red light on side 2 GPIO15
LED_BL1 = Pin(13, Pin.OUT)  # Red light on side 1 GPIO13
LED_HB2 = Pin(14, Pin.OUT)  # High beam side 2 GPIO12
LED_HB1 = Pin(12, Pin.OUT)  # High beam side 1 GPIO14

LED_HDL = PWM(Pin(2, Pin.OUT))  # PWM for headlights controlled by motor board

motor_AIN1 = Pin(5, Pin.OUT)  # GPIO5
motor_AIN2 = Pin(4, Pin.OUT)  # GPIO4
motor_PWM = PWM(Pin(0, Pin.OUT))  # GPIO0

# Variables de contrôle
highBeamActive = False
motionActive = False
motionDirection = 0  # 0 pour l'avant, 1 pour l'arrière
target_speed = 0
actual_speed = 0
acceleration_step = 5

# Configuration du serveur web
srv = MicroWebSrv(webPath="/")

# Configuration du point d'accès Wi-Fi
ap = network.WLAN(network.AP_IF)
ap.active(True)
ap.config(essid=ssid, password=password)

# Fonction de gestion de la page d'accueil
def handle_home(client, request):
    global actual_speed, target_speed, motionDirection, highBeamActive, motionActive

    if request.method == 'GET':
        html = """
        <html>
        <header>
            <style type="text/css">
                body { background: #595B82; }
                #header { height: 30px; margin-left: 2px; margin-right: 2px; background: #d5dfe2; border-top: 1px solid #FFF; border-left: 1px solid #FFF; border-right: 1px solid #333; border-bottom: 1px solid #333; border-radius: 5px; font-family: "arial-verdana", arial; padding-top: 10px; padding-left: 20px; }
                #speed_setting { float: right; margin-right: 10px; }
                #control { height: 200px; margin-top: 4px; margin-left: 2px; margin-right: 2px; background: #d5dfe2; border-top: 1px solid #FFF; border-left: 1px solid #FFF; border-right: 1px solid #333; border-bottom: 1px solid #333; border-radius: 5px; font-family: "arial-verdana", arial; }
                .button { border-top: 1px solid #FFF; border-left: 1px solid #FFF; border-right: 1px solid #333; border-bottom: 1px solid #333; border-radius: 5px; margin: 5px; text-align: center; float: left; padding-top: 20px; height: 50px; background: #FFF }
                .long { width: 30%; }
                .short { width: 100px; }
                #message_box { float: left; margin-bottom: 0px; width: 100%; }
                #speed_slider { width: 300px; }
            </style>
            <script type="text/javascript">
                function updateSpeed(speed) { sendData("updateSpeed?speed=" + speed); }
                function runForward() { var speed = document.getElementById("speed_slider").value; sendData("run?dir=forward&speed=" + speed); }
                function runBackward() { var speed = document.getElementById("speed_slider").value; sendData("run?dir=backward&speed=" + speed); }
                function sendData(uri) {
                    var messageDiv = document.getElementById("message_box");
                    messageDiv.innerHTML = "192.168.4.1/" + uri;
                    var xhr = new XMLHttpRequest();
                    xhr.open('GET', 'http://192.168.4.1/' + uri);
                    xhr.withCredentials = true;
                    xhr.setRequestHeader('Content-Type', 'text/plain');
                    xhr.send();
                }
            </script>
        </header>
        <body>
            <div id="header"> OS-Railway Wifi Hectorrail 141
                <div id="speed_setting"> Speed
                    <input id="speed_slider" type="range" min="0" max="1023" step="10" value="512" onchange="updateSpeed(this.value)">
                </div>
            </div>
            <div id="control">
                <div id="button_backward" class="button long" onclick="runBackward()"> Run backward </div>
                <div id="button_stop" class="button long" onclick="updateSpeed(0)"> Stop </div>
                <div id="button_forward" class="button long" onclick="runForward()"> Run forward </div>
                <div id="button_light_off" class="button long" onclick="sendData('lightoff')"> Headlight off </div>
                <div id="button_light_on" class="button long" onclick="sendData('lighton')"> Headlight on </div>
                <div id="button_emergency" class="button long" onclick="sendData('stop')"> Emergency stop </div>
                <div id="message_box"> </div>
            </div>
        </body>
        </html>
        """
        request.Response.ReturnOk(contentType="text/html", contentCharset="UTF-8", content=html)

# Fonction pour la gestion de la vitesse et du sens du mouvement
def motion_control():
    global motionActive, actual_speed, target_speed, acceleration_step, motionDirection

    if motionActive:
        if actual_speed < target_speed:
            actual_speed = actual_speed + acceleration_step
        elif actual_speed > target_speed:
            actual_speed = actual_speed - acceleration_step

        actual_speed = min(1023, max(0, actual_speed))
        motor_PWM.duty(actual_speed)
    else:
        motor_PWM.duty(0)

# Fonction pour gérer les opérations de moteur
def motor_operation(request, response, args=None):
    global motionActive, actual_speed, target_speed, motionDirection, highBeamActive

    if 'dir' in args and 'speed' in args:
        move_dir = args['dir']
        motion_speed = int(args['speed'])

        if move_dir == "backward":
            if motionDirection != 1 and actual_speed > 100:
                target_speed = 0
            else:
                motionDirection = 1
        else:
            if motionDirection != 0 and actual_speed > 100:
                target_speed = 0
            else:
                motionDirection = 0

        motor_AIN1.value(motionDirection)
        motor_AIN2.value(not motionDirection)
        LED_BL2.value(motionDirection)
        LED_BL1.value(not motionDirection)

        if highBeamActive:
            LED_HB2.value(motionDirection)
            LED_HB1.value(not motionDirection)
        else:
            LED_HB2.value(0)
            LED_HB1.value(0)

        target_speed = motion_speed
        motionActive = True
        response.ReturnOk()
    else:
        response.ReturnBadRequest()

# Fonction pour mettre à jour la vitesse
def update_speed(request, response, args=None):
    global target_speed

    if 'speed' in args:
        target_speed = int(args['speed'])
        response.ReturnOk()
    else:
        response.ReturnBadRequest()

# Fonction pour activer les feux de route
def switch_light_on(request, response, args=None):
    global highBeamActive

    highBeamActive = True
    LED_HB2.value(motionDirection)
    LED_HB1.value(not motionDirection)
    response.ReturnOk()

# Fonction pour éteindre les feux de route
def switch_light_off(request, response, args=None):
    global highBeamActive

    highBeamActive = False
    LED_HB2.value(0)
    LED_HB1.value(0)
    response.ReturnOk()

# Fonction pour arrêter le mouvement
def motion_stop(request, response, args=None):
    global motionActive, actual_speed

    motionActive = False
    motor_PWM.duty(0)
    response.ReturnOk()

# Définir les routes du serveur web
srv.Route('/', 'GET', handle_home)
srv.Route('/run', 'GET', motor_operation)
srv.Route('/stop', 'GET', motion_stop)
srv.Route('/lighton', 'GET', switch_light_on)
srv.Route('/lightoff', 'GET', switch_light_off)
srv.Route('/updateSpeed', 'GET', update_speed)

# Boucle principale
while True:
    motion_control()
    srv.Update()
    time.sleep_ms(10)
