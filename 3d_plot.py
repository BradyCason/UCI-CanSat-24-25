import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import pandas as pd

# Data
data = {
    "GPS_ALTITUDE": [
        171.2, 171.1, 171.1, 171.1, 171.1, 171.1, 171.0, 171.0, 171.0, 171.0,
        170.9, 170.9, 170.9, 170.9, 170.9, 12.0, 12.0, 169.1, 169.1, 169.1,
        169.1, 169.1, 169.1, 169.1, 169.1, 169.1, 169.1, 169.1, 169.1, 169.1
    ],
    "GPS_LATITUDE": [38.2] * 30,
    "GPS_LONGITUDE": [-79.4] * 30
}

# Create DataFrame
df = pd.DataFrame(data)

# Create 3D plot
fig = plt.figure()
ax = fig.add_subplot(111, projection='3d')

# Plot
ax.plot(df["GPS_LONGITUDE"], df["GPS_LATITUDE"], df["GPS_ALTITUDE"], marker='o', color='orange')

# Labels and title
ax.set_xlabel("Longitude (Deg)")
ax.set_ylabel("Latitude (Deg)")
ax.set_zlabel("Altitude (m)")
ax.set_title("GPS Position")

plt.show()
