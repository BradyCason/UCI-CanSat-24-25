import sys
import os
import time
import serial
import threading
from PyQt5 import QtCore, QtGui, QtWidgets, uic
import pyqtgraph as pg
from csv import writer, reader
from datetime import datetime
import pytz

TEAM_ID = "3141"

TELEMETRY_FIELDS = ["TEAM_ID", "MISSION_TIME", "PACKET_COUNT", "MODE", "STATE", "ALTITUDE",
                    "TEMPERATURE", "PRESSURE", "VOLTAGE", "GYRO_R", "GYRO_P", "GYRO_Y", "ACCEL_R",
                    "ACCEL_P", "ACCEL_Y", "MAG_R", "MAG_P", "MAG_Y", "AUTO_GYRO_ROTATION_RATE",
                    "GPS_TIME", "GPS_ALTITUDE", "GPS_LATITUDE", "GPS_LONGITUDE", "GPS_SATS",
                    "CMD_ECHO" ]
GRAPHED_FIELDS = ["PACKET_COUNT", "ALTITUDE", "TEMPERATURE", "PRESSURE", "VOLTAGE", "GYRO_R", 
                  "GYRO_P", "GYRO_Y", "ACCEL_R", "ACCEL_P", "ACCEL_Y", "MAG_R", "MAG_P", "MAG_Y",
                  "AUTO_GYRO_ROTATION_RATE", "GPS_ALTITUDE", "GPS_LATITUDE", "GPS_LONGITUDE", "GPS_SATS"]

current_time = time.time()
local_time = time.localtime(current_time)
readable_time = time.strftime("telemetry_%Y-%m-%d_%H-%M-%S", local_time)

sim = False
running = True
sim_enable = False
telemetry_on = True
calibrate_comp_on = False
set_cam_on = False
csv_indexer = 0

# xbee communication parameters
BAUDRATE = 115200
COM_PORT = 5

SER_DEBUG = False       # Set as True whenever testing without XBee connected
if (not SER_DEBUG):
    # ser = serial.Serial("/dev/tty.usbserial-AR0JQZCB", BAUDRATE, timeout=0.05)
    ser = serial.Serial("COM" + str(COM_PORT), BAUDRATE, timeout=0.05)

# telemetry
# strings as keys and values as values, only last stored
# need to write all commands to csv files by last filled values
telemetry = {}

START_DELIMITER = "~"

class GroundStationWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()

        # Load the UI
        ui_path = os.path.join(os.path.dirname(__file__), "gui", "ground_station.ui")
        uic.loadUi(ui_path, self)

        self.setup_UI()
        self.connect_buttons()

        self.update_timer = QtCore.QTimer()
        self.update_timer.setInterval(100)  # Update every 100ms
        self.update_timer.timeout.connect(self.update)
        self.update_timer.start()

    def setup_UI(self):
        logo_pixmap = QtGui.QPixmap(os.path.join(os.path.dirname(__file__), "gui", "logo.png")).scaled(self.logo.size(), aspectRatioMode=True)
        self.logo.setPixmap(logo_pixmap)
        self.setWindowIcon(QtGui.QIcon(os.path.join(os.path.dirname(__file__), "gui", "logo.png")))
        self.title.setText("CanSat Ground Station - TEAM " + TEAM_ID)

        # graph window, linked to button on rightmost panel
        self.graph_window = GraphWindow()

        # Get telemetry labels
        self.telemetry_labels = {}
        for field in TELEMETRY_FIELDS:
            label = self.telemetry_container_1.findChild(QtWidgets.QLabel, field)
            if (not label):
                label = self.telemetry_container_2.findChild(QtWidgets.QLabel, field)
                if (not label):
                    label = self.simulation_container.findChild(QtWidgets.QLabel, field)
            self.telemetry_labels[field] = label

        # Set First Color of Telemetry Toggle button
        if telemetry_on:
            self.telemetry_toggle_button.setText("Telemetry Toggle: On")
            self.make_button_green(self.telemetry_toggle_button)
        else:
            self.telemetry_toggle_button.setText("Telemetry Toggle: Off")
            self.make_button_red(self.telemetry_toggle_button)

    def connect_buttons(self):
        self.sim_enable_button.clicked.connect(lambda: self.handle_simulation("ENABLE"))
        self.sim_activate_button.clicked.connect(lambda: self.handle_simulation("ACTIVATE"))
        self.sim_disable_button.clicked.connect(lambda: self.handle_simulation("DISABLE"))
        self.set_time_gps_button.clicked.connect(lambda: write_xbee("CMD," + TEAM_ID + ",ST,GPS"))
        self.set_time_utc_button.clicked.connect(lambda: write_xbee("CMD," + TEAM_ID + ",ST," + datetime.now(pytz.timezone("UTC")).strftime("%H:%M:%S")))
        self.calibrate_alt_button.clicked.connect(lambda: write_xbee("CMD," + TEAM_ID + ",CAL"))
        self.set_camera_north_button.clicked.connect(self.set_camera_north_toggle)
        self.release_payload_button.clicked.connect(lambda: write_xbee("CMD," + TEAM_ID + ",MEC,PAYLOAD,ON"))
        self.calibrate_comp_button.clicked.connect(self.calibrate_comp_toggle)
        self.telemetry_toggle_button.clicked.connect(self.toggle_telemetry)
        self.show_graphs_button.clicked.connect(self.graph_window.show)

        # Connect non-sim buttons to update sim button colors
        self.set_time_gps_button.clicked.connect(self.non_sim_button_clicked)
        self.set_camera_north_button.clicked.connect(self.non_sim_button_clicked)
        self.set_time_utc_button.clicked.connect(self.non_sim_button_clicked)
        self.calibrate_alt_button.clicked.connect(self.non_sim_button_clicked)
        self.release_payload_button.clicked.connect(self.non_sim_button_clicked)
        self.telemetry_toggle_button.clicked.connect(self.non_sim_button_clicked)

    def update(self):
        global telemetry
        global telemetry_on

        if telemetry_on:
            for field in TELEMETRY_FIELDS:
                if field != "TEAM_ID":
                    self.telemetry_labels[field].setText(telemetry[field])

            self.graph_window.update()

    def make_button_green(self, button):
        button.setStyleSheet("QPushButton{background-color: rgba(40, 167, 69, 1);} QPushButton:hover{background-color: rgba(36, 149, 62, 1);}")

    def make_button_red(self, button):
        button.setStyleSheet("QPushButton{background-color: rgba(220, 53, 69, 1);} QPushButton:hover{background-color: rgba(200, 45, 59, 1);}")

    def make_button_blue(self, button):
        button.setStyleSheet("QPushButton{background-color: rgba(33,125,182,1);} QPushButton:hover{background-color: rgba(27, 100, 150, 1);}")

    def handle_simulation(self, cmd):
        global sim, sim_enable, csv_indexer

        if cmd == "ACTIVATE" and sim_enable == False or cmd == "ENABLE" and sim_enable == True:
            return
        
        write_xbee("CMD," + TEAM_ID + ",SIM," + cmd)

        if cmd == "ENABLE":
            sim_enable = True
        elif cmd == "DISABLE":
            sim_enable = False
            sim = False
        elif cmd == "ACTIVATE":
            sim_enable = False
            sim = True
            csv_indexer = 0
        
        self.update_sim_button_colors()

        if cmd == "ACTIVATE":
            # Wait 1 second to let the Payload receive the command
            # before sending simp data
            time.sleep(1)

    def update_sim_button_colors(self):
        global sim, sim_enable

        if (sim):
            self.make_button_blue(self.sim_enable_button)
            self.make_button_blue(self.sim_activate_button)
            self.make_button_red(self.sim_disable_button)
        elif (sim_enable):
            self.make_button_blue(self.sim_enable_button)
            self.make_button_green(self.sim_activate_button)
            self.make_button_blue(self.sim_disable_button)
        else:
            self.make_button_green(self.sim_enable_button)
            self.make_button_blue(self.sim_activate_button)
            self.make_button_blue(self.sim_disable_button)

    def non_sim_button_clicked(self):
        global sim_enable
        sim_enable = False
        self.update_sim_button_colors()
    
    def set_camera_north_toggle(self):
        global set_cam_on
        set_cam_on = not set_cam_on
        
        if set_cam_on:
            write_xbee("CMD," + TEAM_ID + ",SCN,ON")
            self.set_camera_north_button.setText("Camera Is North")
            self.make_button_green(self.set_camera_north_button)
        else:
            write_xbee("CMD," + TEAM_ID + ",SCN,OFF")
            self.set_camera_north_button.setText("Set Camera North")
            self.make_button_blue(self.set_camera_north_button)

    def calibrate_comp_toggle(self):
        global calibrate_comp_on
        calibrate_comp_on = not calibrate_comp_on

        if calibrate_comp_on:
            write_xbee("CMD,"+ TEAM_ID + ",CC,ON")
            self.calibrate_comp_button.setText("Stop Calibrating Compass")
            self.make_button_green(self.calibrate_comp_button)
        else:
            write_xbee("CMD,"+ TEAM_ID + ",CC,OFF")
            self.calibrate_comp_button.setText("Calibrate Compass")
            self.make_button_blue(self.calibrate_comp_button)

    def toggle_telemetry(self):
        global telemetry_on
        telemetry_on = not telemetry_on

        if telemetry_on:
            write_xbee("CMD,"+ TEAM_ID + ",CX,ON")
            self.telemetry_toggle_button.setText("Telemetry Toggle: On")
            self.make_button_green(self.telemetry_toggle_button)
        else:
            write_xbee("CMD,"+ TEAM_ID + ",CX,OFF")
            self.telemetry_toggle_button.setText("Telemetry Toggle: Off")
            self.make_button_red(self.telemetry_toggle_button)


class GraphWindow(pg.GraphicsLayoutWidget):
    def __init__(self):
        super().__init__()

        self.layout = QtWidgets.QVBoxLayout()
        self.setLayout(self.layout)
        self.setStyleSheet("background-color: white; border:4px solid rgb(0, 0, 0);")
        self.setWindowTitle("UCI CanSat Live Telemetry")
        self.setWindowIcon(QtGui.QIcon(os.path.join(os.path.dirname(__file__), "gui", "logo.png")))

        # set up arrays for data that will be graphed
        self.graph_data = {}
        for field in GRAPHED_FIELDS:
            self.graph_data[field] = []

        # initialize graphs
        self.graphs = {}
        for field in GRAPHED_FIELDS:
            self.graphs[field] = self.addPlot(title=field)
            self.graphs[field].curve = self.graphs[field].plot(pen={'width': 3}, symbol='o')

            # go to next row every 4 graphs
            if (self.itemIndex(self.graphs[field]) + 1) % 4 == 0:
                self.nextRow()

        self.previous_time = ""

    def update(self):
        global telemetry

        if self.previous_time != telemetry["GPS_TIME"]:

            for field in GRAPHED_FIELDS:
                if telemetry[field] != "N/A":
                    try:
                        self.graph_data[field].append(float(telemetry[field]))
                        if len(self.graph_data[field]) > 50:
                            self.graph_data[field].pop(0)
                    except:
                        pass
                    self.graphs[field].curve.setData(self.graph_data[field])

                    # if len(self.graph_data[field]) > 50:
                    #     self.graph_data[field] = self.graph_data[field][-50:]

            self.previous_time = telemetry["GPS_TIME"]

def calc_checksum(data):
    return sum(data.encode()) % 256

def verify_checksum(data, checksum):
    return checksum == calc_checksum(data)

def parse_xbee(data):
    global sim, telemetry

    for i in range(len(data)):
        telemetry[TELEMETRY_FIELDS[i]] = data[i]

    # if data[3] == "S":
    #     sim = True
    # else:
    #     sim = False

    # Add data to csv file
    file = os.path.join(os.path.dirname(__file__), "Flight_" + TEAM_ID + '.csv')
    with open(file, 'a', newline='') as f_object:
        writer_object = writer(f_object)
        writer_object.writerow(telemetry.values())

def read_xbee():
    buffer = ""
    while True:     # Keep running as long as the serial connection is open
        if ser.inWaiting() > 0:
            buffer += ser.read(ser.inWaiting()).decode()

            start_idx = buffer.find(START_DELIMITER)
            end_idx = buffer.find("\n")
            if start_idx == -1 or end_idx == -1:
                # Wait for a full packet
                continue

            frame = buffer[start_idx + 1:end_idx].strip()
            buffer = buffer[end_idx + 1:]

            try:
                data, checksum = frame.rsplit(",", 1)
                if verify_checksum(data, float(checksum)):
                    if len(data.split(",")) == 25:
                        parse_xbee(data.split(","))
                    else:
                        print("Incorrect number of fields in frame: ", frame)
                else:
                    print("Failed to read frame:", frame)
            except:
                print("Error reading frame: ", frame)

            # start_byte = ser.read(1)
            # if start_byte != START_DELIMITER:
            #     print(start_byte.decode())

            # if start_byte == START_DELIMITER:
            #     time.sleep(0.1)
            #     frame = ser.read_until(b"\n").decode().strip()
            #     try:
            #         data, checksum = frame.rsplit(",", 1)
            #         if verify_checksum(data, float(checksum)):
            #             parse_xbee(data.split(","))
            #         else:
            #             print("Failed to read frame:", frame)
            #     except:
            #         print("Failed to read frame:", frame)

def write_xbee(cmd):
    # Frame Format: ~<data>,<checksum>

    '''
        Commands:
        CMD,<TEAM_ID>,CX,<ON_OFF>               Payload Telemetry On/Off Command
        CMD,<TEAM_ID>,ST,<UTC_TIME>|GPS         Set Time
        CMD,<TEAM_ID>,SIM,<MODE>                Simulation Mode Control Command
        CMD,<TEAM ID>,SIMP,<PRESSURE>           Simulated Pressure Data (to be used in Simulation Mode only)
        CMD,<TEAM ID>,CAL                       Calibrate Altitude to Zero
        CMD,<TEAM ID>,MEC,<DEVICE>,<ON_OFF>     Mechanism actuation command

        Additional Commands:
        CMD,<TEAM_ID>,BCN,<ON_OFF>              Turns the beacon on or off
    '''

    # Create Packet
    checksum = calc_checksum(cmd)
    frame = f"{START_DELIMITER}{cmd},{checksum:02X}"

    # Send to XBee
    if (not SER_DEBUG):
        ser.write(frame.encode())
    print("Packet Sent: " + cmd)

def send_simp_data():
    global sim, running
    global csv_indexer
    csv_file = open(os.path.join(os.path.dirname(__file__), "pres.csv"), 'r')
    csv_lines = csv_file.readlines()
    csv_indexer = 0
    while running:
        if sim and csv_indexer < len(csv_lines):
            csv_num = str(csv_lines[csv_indexer].strip())
            write_xbee('CMD,' + TEAM_ID + ',SIMP,' + str(csv_num))
            csv_indexer += 1
            
        time.sleep(1)




def main():
    # Create new csv file with header
    file = os.path.join(os.path.dirname(__file__), "Flight_" + TEAM_ID + '.csv')
    with open(file, 'w', newline='') as f_object:
        writer_object = writer(f_object)
        writer_object.writerow(TELEMETRY_FIELDS)

    # Run the app
    app = QtWidgets.QApplication(sys.argv)
    w = GroundStationWindow()

    if (not SER_DEBUG):
        threading.Thread(target=read_xbee, daemon=True).start()
    threading.Thread(target=send_simp_data, daemon=True).start()

    w.show()
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()