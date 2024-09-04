import sys
import os
import time
import serial
from PyQt5 import QtCore, QtGui, QtWidgets, uic
import pyqtgraph as pg

TEAM_ID = "2057"

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
telemetry_on = False
csv_indexer = 0

# xbee communication parameters
BAUDRATE = 115200
COM_PORT = 6

MAC_ADDR = ""

#ser = serial.Serial("COM" + str(COM_PORT), BAUDRATE, timeout=0.05)

# telemetry
# strings as keys and values as values, only last stored
# need to write all commands to csv files by last filled values
telemetry = {}

class GroundStationWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()

        # Load the UI
        ui_path = os.path.join(os.path.dirname(__file__), "gui", "ground_station.ui")
        uic.loadUi(ui_path, self)

        self.setup_UI()
        self.connect_buttons()

        # start timer, check for updates to telemetry based on interval
        # self.timer = QtCore.QTimer()
        # self.timer.setInterval(20)
        # self.timer.timeout.connect(self.update)
        # self.timer.start()

    def setup_UI(self):
        logo_pixmap = QtGui.QPixmap(os.path.join(os.path.dirname(__file__), "gui", "logo.png")).scaled(self.logo.size(), aspectRatioMode=True)
        self.logo.setPixmap(logo_pixmap)
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

    def connect_buttons(self):
        self.sim_enable_button.clicked.connect(lambda: self.handle_simulation("ENABLE"))
        self.sim_activate_button.clicked.connect(lambda: self.handle_simulation("ACTIVATE"))
        self.sim_disable_button.clicked.connect(lambda: self.handle_simulation("DISABLE"))
        self.set_time_button.clicked.connect(lambda: write_xbee("CMD," + TEAM_ID + ",ST,GPS"))
        self.calibrate_alt_button.clicked.connect(lambda: write_xbee("CMD," + TEAM_ID + ",CAL"))
        # self.override_state_1_button.clicked.connect(None)
        # self.override_state_2_button.clicked.connect(None)
        self.telemetry_toggle_button.clicked.connect(self.toggle_telemetry)
        self.show_graphs_button.clicked.connect(self.graph_window.show)

    def update(self):
        global telemetry

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
        global sim, sim_enable

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
        
        self.update_sim_button_colors()

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
            self.make_button_red(self.telemetry_toggle_button)


class GraphWindow(pg.GraphicsLayoutWidget):
    def __init__(self):
        super().__init__()

        self.layout = QtWidgets.QVBoxLayout()
        self.setLayout(self.layout)
        self.setStyleSheet("background-color: white; border:4px solid rgb(0, 0, 0);")

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
                    except:
                        pass
                    self.graphs[field].curve.setData(self.graph_data[field])

                    if len(self.graph_data[field]) > 50:
                        self.graph_data[field] = self.graph_data[field][-50:]

            self.previous_time = telemetry["GPS_TIME"]


def parse_xbee(xbee_message):
    pass

def read_xbee():
    pass

def write_xbee(cmd):
    w.update_sim_button_colors()

    # Frame Format: ~<data>,<checksum>

    '''
        Commands:
        CMD,<TEAM_ID>,CX,<ON_OFF>               Payload Telemetry On/Off Command
        CMD,<TEAM_ID>,ST,<UTC_TIME>|GPS         Set Time
        CMD,<TEAM_ID>,SIM,<MODE>                Simulation Mode Control Command
        CMD,<TEAM ID>,SIMP,<PRESSURE>           Simulated Pressure Data (to be used in Simulation Mode only)
        CMD,<TEAM ID>,CAL                       Calibrate Altitude to Zero
        CMD,<TEAM ID>,MEC,<DEVICE>,<ON_OFF>     Mechanism actuation command
    '''

    # Create Packet
    start_delimiter = 0x7E
    checksum = sum(cmd.encode()) % 256
    frame = f"{chr(start_delimiter)}{cmd},{checksum:02X}"

    # Send to XBee
    #ser.write(frame.encode())
    print("Packet Sent: " + cmd)


if __name__ == "__main__":

    # Run the app
    app = QtWidgets.QApplication(sys.argv)
    w = GroundStationWindow()
    w.show()
    write_xbee("CMD,something,somethings")
    sys.exit(app.exec_())