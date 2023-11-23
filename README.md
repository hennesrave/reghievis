# RegHieVis: Region-based Uncertainty in Hierarchically Clustered Ensemble Volumes
## Setup
1. Clone the repository
```
git clone https://github.com/hennesrave/reghievis.git
```
2. Download [Qt5](https://www.qt.io/) sources
3. Download [Eigen](https://eigen.tuxfamily.org/) (version 3.4.0) and copy the `Eigen` folder to `reghievis/external`
4. Build the program using [CMake](https://cmake.org/) (Change the `Qt5_DIR` path to your local Qt5 installation)
```
cd reghievis
mkdir build
cd build
cmake .. -DQt5_DIR:STRING="C:\Qt\5.15.2\msvc2019_64\lib\cmake\Qt5"
cmake --build . --config Release
```
5. If everything worked fine, the executable will be located in `reghievis/build/Release/RegHieVis.exe`
6. **(optional)** Run `windeployqt.exe` to copy the required DLLs (Again, change the path to your Qt5 installation)
```
C:\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe .\Release\RegHieVis.exe
```
7. **(optional)** Start the program! If you have no suitable dataset, you can try passing "teardrop" or "tangle" as a command line argument
```
.\Release\RegHieVis.exe teardrop
```

## Scripts
The `scripts` folder contains the script used for the comparison of hierarchical clustering methods.
