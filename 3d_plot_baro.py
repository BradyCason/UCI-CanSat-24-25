import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import pandas as pd

# Data
data = {
    "GPS_ALTITUDE": [
# -0.3,
# -0.3,
# -0.3,
# -0.2,
# -0.2,
# -0.3,
# -0.2,
# -0.1,
# -0.3,
# 0,
# -0.2,
# -0.2,
# -0.2,
# -0.2,
# -0.3,
# -0.2,
# -0.3,
# -0.3,
# -0.1,
-0.1,
-0.2,
-43.7,
-129.4,
16.2,
261.2,
340.3,
398.2,
423.5,
437.5, 441.6, 435.3
    ],
    "GPS_LATITUDE": [38.2] * 12,
    "GPS_LONGITUDE": [-79.4] * 12
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
ax.set_title("Position Using Barometric Pressure")

plt.show()
