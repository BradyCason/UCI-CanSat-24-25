import sys
import os
from PyQt5 import QtCore, QtGui, QtWidgets, uic

class GroundStationWindow(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()

        # Load the UI
        ui_path = os.path.join(os.path.dirname(__file__), "gui", "ground_station.ui")
        uic.loadUi(ui_path, self)

        self.setup_UI()

    def setup_UI(self):
        logo_pixmap = QtGui.QPixmap(os.path.join(os.path.dirname(__file__), "gui", "logo.png")).scaled(self.logo.size(), aspectRatioMode=True)
        self.logo.setPixmap(logo_pixmap)

if __name__ == "__main__":

    # Run the app
    app = QtWidgets.QApplication(sys.argv)
    w = GroundStationWindow()
    w.show()
    sys.exit(app.exec_())