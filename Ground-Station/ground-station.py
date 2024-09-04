import sys
import os
import time
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

# xbee communication parameters
BAUDRATE = 115200
COM_PORT = 6
COM_PORT2 = 8

MAC_ADDR_P = ""
MAC_ADDR_C = ""

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
        self.show_graphs_button.clicked.connect(self.graph_window.show)
        
    def setLabel(self, field, value):
        self.telemetry_labels[field].setText(value)


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


if __name__ == "__main__":

    # Run the app
    app = QtWidgets.QApplication(sys.argv)
    w = GroundStationWindow()
    w.show()
    sys.exit(app.exec_())