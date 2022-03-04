build: STARELite.cpp 
	c++ -g -fPIC -L/usr/local/lib/ -L/usr/lib/x86_64-linux-gnu/ -I/usr/local/include/STARE -I/usr/include/spatialite/ -std=c++11 -shared STARELite.cpp -o STARELite.so -lspatialite -lSTARE
