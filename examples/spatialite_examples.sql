SELECT load_extension('mod_spatialite');

DROP TABLE IF EXISTS points;
CREATE TABLE points(x double, y double, geom blob);

INSERT INTO points (x, y)  VALUES(0, 0);
INSERT INTO points (x, y)  VALUES(0, 1);
INSERT INTO points (x, y)  VALUES(0, 50);
INSERT INTO points (x, y)  VALUES(0, 2);

UPDATE points SET geom=MakePoint(x, y, 4326);

SELECT ST_INTERSECTS(geom, ST_MakePolygon(MakeCircle(0, 0, 10))) FROM points;

## Making
SELECT InitSpatialMetaData(0, 'WGS84');
SELECT RecoverGeometryColumn('cities', 'geom', 4326, 'POINT');

UPDATE cities SET geometry=GeomFromEWKB(geometry) FROM cities;
SELECT RecoverGeometryColumn('cities', 'geometry', 4326, 'POINT');
