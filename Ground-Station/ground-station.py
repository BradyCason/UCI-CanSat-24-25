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
import matplotlib.pyplot as plt

TEAM_ID = "3141"

TELEMETRY_FIELDS = ["TEAM_ID", "MISSION_TIME", "PACKET_COUNT", "MODE", "STATE", "ALTITUDE",
                    "TEMPERATURE", "PRESSURE", "VOLTAGE", "GYRO_R", "GYRO_P", "GYRO_Y", "ACCEL_R",
                    "ACCEL_P", "ACCEL_Y", "MAG_R", "MAG_P", "MAG_Y", "AUTO_GYRO_ROTATION_RATE",
                    "GPS_TIME", "GPS_ALTITUDE", "GPS_LATITUDE", "GPS_LONGITUDE", "GPS_SATS",
                    "CMD_ECHO", "HEADING", "MAX_ALTITUDE", "PAYLOAD_RELEASED"]
GRAPHED_FIELDS = ["PACKET_COUNT", "ALTITUDE", "TEMPERATURE", "PRESSURE", "VOLTAGE", "GYRO_R", 
                  "GYRO_P", "GYRO_Y", "ACCEL_R", "ACCEL_P", "ACCEL_Y", "MAG_R", "MAG_P", "MAG_Y",
                  "AUTO_GYRO_ROTATION_RATE", "GPS_ALTITUDE", "GPS_LATITUDE", "GPS_LONGITUDE", "GPS_SATS", "HEADING"]

current_time = time.time()
local_time = time.localtime(current_time)
readable_time = time.strftime("telemetry_%Y-%m-%d_%H-%M-%S", local_time)

sim = False
running = True
sim_enable = False
telemetry_on = True
calibrate_comp_on = False
north_cam_on = False
csv_indexer = 0

packet_count = 0
# last_recieved_packet = -1
# last_recieved_compass_packet = -1
packets_sent = 0

# xbee communication parameters
BAUDRATE = 115200
COM_PORT = [6,3]
NUM_XBEES = 1

SER_DEBUG = False       # Set as True whenever testing without XBee connected

ser = [None, None]
serialConnected = [False, False]
def connect_Serial(xbee_num):
    global ser
    global serialConnected
    if (not SER_DEBUG):
        try:
            # ser = serial.Serial("/dev/tty.usbserial-AR0JQZCB", BAUDRATE, timeout=0.05)
            ser[xbee_num] = serial.Serial("COM" + str(COM_PORT[xbee_num]), BAUDRATE, timeout=0.05)
            serialConnected[xbee_num] = True
            print("Connected to Xbee: " + str(xbee_num))
        except serial.serialutil.SerialException as e:
            if (serialConnected[xbee_num]):
                print("Could not connect to Xbee " + str(xbee_num) + f": {e}")
            serialConnected[xbee_num] = False

def disconnect_Serial(xbee_num):
    global ser
    global serialConnected
    if (not SER_DEBUG):
        try:
            if serialConnected[xbee_num] and ser[xbee_num].is_open:
                ser[xbee_num].close()
                print("Disconnected from Xbee: " + str(xbee_num))
            serialConnected[xbee_num] = False
        except Exception as e:
            print(f"Error while disconnecting Xbee {xbee_num}: {e}")
            serialConnected[xbee_num] = False

# telemetry
# strings as keys and values as values, only last stored
# need to write all commands to csv files by last filled values
telemetry = {}

START_DELIMITER = "~"

class GroundStationWindow(QtWidgets.QMainWindow):
    def __init__(self):
        '''
        Initialize the Ground Station Window, and start timer loop for updating UI
        '''
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

        self.payload_released = False

        self.compass_plotter = None

    def setup_UI(self):
        '''
        Set UI to initial State
        '''
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
        '''
        Connect Buttons to their functions
        '''
        self.sim_enable_button.clicked.connect(lambda: self.handle_simulation("ENABLE"))
        self.sim_activate_button.clicked.connect(lambda: self.handle_simulation("ACTIVATE"))
        self.sim_disable_button.clicked.connect(lambda: self.handle_simulation("DISABLE"))
        self.reset_state_button.clicked.connect(self.reset_state)
        self.set_time_gps_button.clicked.connect(lambda: write_xbee("CMD," + TEAM_ID + ",ST,GPS"))
        self.set_time_utc_button.clicked.connect(lambda: write_xbee("CMD," + TEAM_ID + ",ST," + datetime.now(pytz.timezone("UTC")).strftime("%H:%M:%S")))
        self.calibrate_alt_button.clicked.connect(lambda: write_xbee("CMD," + TEAM_ID + ",CAL"))
        self.set_camera_north_button.clicked.connect(lambda: write_xbee("CMD," + TEAM_ID + ",SCN"))
        self.activate_north_cam_button.clicked.connect(self.camera_north_toggle)
        self.release_payload_button.clicked.connect(self.release_payload_clicked)
        self.calibrate_comp_button.clicked.connect(self.calibrate_comp_toggle)
        self.telemetry_toggle_button.clicked.connect(self.toggle_telemetry)
        self.show_graphs_button.clicked.connect(self.graph_window.show)

        # Connect non-sim buttons to update sim button colors
        self.reset_state_button.clicked.connect(self.non_sim_button_clicked)
        self.set_time_gps_button.clicked.connect(self.non_sim_button_clicked)
        self.set_camera_north_button.clicked.connect(self.non_sim_button_clicked)
        self.set_time_utc_button.clicked.connect(self.non_sim_button_clicked)
        self.calibrate_alt_button.clicked.connect(self.non_sim_button_clicked)
        self.release_payload_button.clicked.connect(self.non_sim_button_clicked)
        self.telemetry_toggle_button.clicked.connect(self.non_sim_button_clicked)

    def update(self):
        '''
        Set telemetry fields to most recent data
        '''
        global telemetry
        global telemetry_on

        if len(telemetry.keys()) > 0 and telemetry_on:
            for field in TELEMETRY_FIELDS:
                if field != "TEAM_ID":
                    self.telemetry_labels[field].setText(telemetry[field])

            self.graph_window.update()

    def make_button_green(self, button):
        '''
        Takes in a button object and makes the background green
        '''
        button.setStyleSheet("QPushButton{background-color: rgba(40, 167, 69, 1);} QPushButton:hover{background-color: rgba(36, 149, 62, 1);}")

    def make_button_red(self, button):
        '''
        Takes in a button object and makes the background red
        '''
        button.setStyleSheet("QPushButton{background-color: rgba(220, 53, 69, 1);} QPushButton:hover{background-color: rgba(200, 45, 59, 1);}")

    def make_button_blue(self, button):
        '''
        Takes in a button object and makes the background blue
        '''
        button.setStyleSheet("QPushButton{background-color: rgba(33,125,182,1);} QPushButton:hover{background-color: rgba(27, 100, 150, 1);}")

    def handle_simulation(self, cmd):
        '''
        Send a simulation command: ("ACTIVATE", "ENABLE", "DISABLE")
        '''
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
        '''
        Set simulation buttons to the correct colors based off of
        if it is active, enabled, or disabled
        '''
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
        '''
        Disable simulation when any button is pressed
        '''
        global sim_enable
        sim_enable = False
        self.update_sim_button_colors()
    
    def reset_state(self):
        global packet_count
        packet_count = 0
        write_xbee("CMD," + TEAM_ID + ",RST")

    def camera_north_toggle(self):
        '''
        Handle "activate north camera" button press
        '''
        global north_cam_on
        north_cam_on = not north_cam_on
        
        if north_cam_on:
            write_xbee("CMD," + TEAM_ID + ",MEC,CAM,ON")
            self.activate_north_cam_button.setText("Deactivate North Camera")
            self.make_button_green(self.activate_north_cam_button)
        else:
            write_xbee("CMD," + TEAM_ID + ",MEC,CAM,OFF")
            self.activate_north_cam_button.setText("Activate North Camera")
            self.make_button_blue(self.activate_north_cam_button)

    def calibrate_comp_toggle(self):
        '''
        Handle "calibrate compass" button press
        '''
        global calibrate_comp_on
        calibrate_comp_on = not calibrate_comp_on

        if calibrate_comp_on:
            write_xbee("CMD,"+ TEAM_ID + ",CC,ON")
            self.calibrate_comp_button.setText("Stop Calibrating Compass")
            self.make_button_green(self.calibrate_comp_button)

            self.compass_plotter = CalibrateCompassPlotter()
        else:
            write_xbee("CMD,"+ TEAM_ID + ",CC,OFF")
            self.calibrate_comp_button.setText("Calibrate Compass")
            self.make_button_blue(self.calibrate_comp_button)

            self.compass_plotter.close_plot()

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

    def release_payload_clicked(self):
        self.payload_released = not self.payload_released

        if self.payload_released:
            write_xbee("CMD," + TEAM_ID + ",MEC,PAYLOAD,ON")
            self.release_payload_button.setText("Reset Payload Release")
            self.make_button_red(self.release_payload_button)
        else:
            write_xbee("CMD," + TEAM_ID + ",MEC,PAYLOAD,OFF")
            self.release_payload_button.setText("Release Payload")
            self.make_button_blue(self.release_payload_button)

class CalibrateCompassPlotter:
    def __init__(self):
        # Data storage
        self.x_vals, self.y_vals = [], []
        self.x_offsets, self.y_offsets = [], []

        # Setup plot
        plt.ion()
        self.fig, self.ax = plt.subplots()
        self.fig.canvas.mpl_connect('close_event', self.on_close)
        self.val_scatter = self.ax.scatter(self.x_vals, self.y_vals, color='blue', label='Compass Reading')
        self.offset_scatter = self.ax.scatter(self.x_offsets, self.y_offsets, color='red', label='Offset')
        plt.gcf().canvas.manager.set_window_title('Compass Calibration Data')

        self.ax.set_xlim(-0.75, 0.75)
        self.ax.set_ylim(-0.75, 0.75)
        self.ax.set_aspect('equal', adjustable='box')
        self.ax.grid(True)
        self.ax.legend()

        self.x_min_line = None
        self.x_max_line = None
        self.y_min_line = None
        self.y_max_line = None
        self.heading_text = None

        self.manual_closing = False
    
    def add_points(self, points):
        # Append new data points
        self.x_vals.append(points[0])
        self.y_vals.append(points[1])
        self.x_offsets.append(points[2])
        self.y_offsets.append(points[3])

        # Update plot data
        self.val_scatter.set_offsets(list(zip(self.x_vals, self.y_vals)))
        self.offset_scatter.set_offsets([(points[2], points[3])])

        if self.x_min_line:
            self.x_min_line.remove()
        if self.x_max_line:
            self.x_max_line.remove()
        if self.y_min_line:
            self.y_min_line.remove()
        if self.y_max_line:
            self.y_max_line.remove()
        if self.heading_text:
            self.heading_text.remove()
        self.x_min_line = self.ax.axvline(x=points[4], color='green', linestyle='--', label='X Min')
        self.x_max_line = self.ax.axvline(x=points[5], color='green', linestyle='--', label='X Max')
        self.y_min_line = self.ax.axhline(y=points[6], color='purple', linestyle='--', label='Y Min')
        self.y_max_line = self.ax.axhline(y=points[7], color='purple', linestyle='--', label='Y Max')
        self.heading_text = self.ax.text(0, 1, f'Heading:{str(int(points[8]))}°', fontsize=12, color='black',
                                         horizontalalignment='left', verticalalignment='bottom', transform=self.ax.transAxes)


        # Refresh plot
        self.ax.relim()
        self.ax.autoscale_view()
        global calibrate_comp_on
        if calibrate_comp_on:
            plt.draw()

    def close_plot(self):
        # Close the plot window
        self.manual_closing = True
        plt.close(self.fig)

    def on_close(self, args):
        global w
        global calibrate_comp_on
        if (not self.manual_closing):
            w.calibrate_comp_toggle()

class GraphWindow(pg.GraphicsLayoutWidget):
    def __init__(self):
        '''
        Initialize Graphing Window UI
        '''
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
        '''
        Add new data points to graphs if necessary
        '''
        global telemetry

        if self.previous_time != telemetry["MISSION_TIME"]:

            for field in GRAPHED_FIELDS:
                if telemetry[field] != "N/A":
                    try:
                        self.graph_data[field].append(float(telemetry[field]))
                        if len(self.graph_data[field]) > 50:
                            self.graph_data[field].pop(0)
                    except:
                        print("Error")
                    self.graphs[field].curve.setData(self.graph_data[field])

                    # if len(self.graph_data[field]) > 50:
                    #     self.graph_data[field] = self.graph_data[field][-50:]

            self.previous_time = telemetry["MISSION_TIME"]

def calc_checksum(data):
    '''
    Calculate Checksum based off of packet data
    '''
    return sum(data.encode()) % 256

def verify_checksum(data, checksum):
    '''
    Return a boolean of if the checksum is correct
    '''
    return checksum == calc_checksum(data)

def parse_xbee(data):
    '''
    Parse the data from an incoming Xbee packet
    '''
    global sim, telemetry, packet_count #, last_recieved_packet

    # Ensure only recieving each packet once
    # sent_packet_count = int(data[TELEMETRY_FIELDS.index("PACKET_COUNT")])
    # if sent_packet_count == last_recieved_packet:
    #     return
    # last_recieved_packet = sent_packet_count

    packet_count += 1
    telemetry["PACKET_COUNT"] = str(packet_count)

    for i in range(len(data) - 1):
        if TELEMETRY_FIELDS[i] != "PACKET_COUNT":
            telemetry[TELEMETRY_FIELDS[i]] = data[i]

    # if data[3] == "S":
    #     sim = True
    # else:
    #     sim = False

    # Add data to csv file
    file = os.path.join(os.path.dirname(__file__), "Flight_" + TEAM_ID + "_" + readable_time +'.csv')
    with open(file, 'a', newline='') as f_object:
        writer_object = writer(f_object)
        writer_object.writerow(list(telemetry.values()) + [data[-1]])

def read_xbee():
    '''
    Read packets from the Xbee module
    '''
    buffer = ["", ""]
    global serialConnected
    while True:     # Keep running as long as the serial connection is open
        for xbee_num in range(NUM_XBEES):
            if not serialConnected[xbee_num]:
                connect_Serial(xbee_num)
                continue

            try:
                if ser[xbee_num].inWaiting() > 0:
                    buffer[xbee_num] += ser[xbee_num].read(ser[xbee_num].inWaiting()).decode(errors='replace')

                    start_idx = buffer[xbee_num].find(START_DELIMITER)
                    end_idx = buffer[xbee_num].find("\n", start_idx)
                    next_start = buffer[xbee_num].find(START_DELIMITER, start_idx + 1)

                    if next_start != -1 and (end_idx == -1 or next_start < end_idx):
                        frame_end = next_start
                    elif end_idx != -1:
                        frame_end = end_idx
                    else:
                        # Wait for a full packet
                        continue

                    frame = buffer[xbee_num][start_idx + 1:frame_end].strip()
                    buffer[xbee_num] = buffer[xbee_num][frame_end + 1:]

                    try:
                        data, checksum = frame.rsplit(",", 1)
                        if verify_checksum(data, float(checksum)):

                            split_data = data.split(",")
                            if len(split_data) == 29:
                                parse_xbee(split_data)
                            elif len(split_data) == 10 and calibrate_comp_on:
                                # global last_recieved_compass_packet
                                if int(split_data[0]): # != last_recieved_compass_packet:
                                    # last_recieved_compass_packet = int(split_data[0])
                                    global w
                                    if (w.compass_plotter):
                                        w.compass_plotter.add_points([float(x) for x in split_data[1:]])
                            else:
                                print("Incorrect number of fields in frame: ", frame)
                        else:
                            print("Failed to read frame:", frame)
                    except Exception as e:
                        print(e)
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

                serialConnected[xbee_num] = True
            except serial.serialutil.SerialException as e:
                if (serialConnected[xbee_num]):
                    print("Serial " + str(xbee_num) + f" Connection Lost: {e}")
                serialConnected[xbee_num] = False
            except OSError as e:
                if (serialConnected[xbee_num]):
                    print("Serial " + str(xbee_num) + f" Connection Lost: {e}")
                serialConnected[xbee_num] = False

        time.sleep(0.1)
            

def write_xbee(cmd):
    '''
    Write commands to the Xbee
    '''
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
    global packets_sent
    packets_sent += 1
    checksum = calc_checksum(f"{packets_sent},{cmd}")
    frame = f"{START_DELIMITER}{packets_sent},{cmd},{checksum:02X}"

    # Send to XBee
    if (not SER_DEBUG):
        sent = []
        for xbee_num in range(2):
            try:
                if (ser[xbee_num]):
                    ser[xbee_num].write(frame.encode())
                    sent.append(xbee_num)
                    time.sleep(0.1)
            except serial.serialutil.SerialException as e:
                print(f"Packet Not Sent on Xbee {xbee_num}: {e}")
        
        print(f"Packet Sent on Xbees {sent}: {cmd}")

def send_simp_data():
    '''
    Send simulated pressure data from the csv file at 1 Hz
    '''
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
    for xbee_num in range(NUM_XBEES):
        connect_Serial(xbee_num)

    # Create new csv file with header
    file = os.path.join(os.path.dirname(__file__), "Flight_" + TEAM_ID + "_" + readable_time + '.csv')
    with open(file, 'w', newline='') as f_object:
        writer_object = writer(f_object)
        writer_object.writerow(TELEMETRY_FIELDS + ["CAM_DIRECTION"])

    # Run the app
    app = QtWidgets.QApplication(sys.argv)
    global w
    w = GroundStationWindow()

    if (not SER_DEBUG):
        threading.Thread(target=read_xbee, daemon=True).start()
    threading.Thread(target=send_simp_data, daemon=True).start()

    w.show()
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()