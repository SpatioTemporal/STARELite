/*
** 2020-09-93
**
**  Author: Niklas Griessbaum
**  griessbaum@ucsb.edu
**
******************************************************************************
**
** This SQLite extension implements bindings to basic STARE functionalites
*/


#include "sqlite3ext.h"
extern "C" SQLITE_EXTENSION_INIT1

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <gaiageo.h>
#include "STARE.h"


static STARE stare;


/*
** Implementation of the stare_from_lonlat() function.
**
** Lookup STARE index value subject to lon, lat and resolution
*/


static void stare_from_lonlat(sqlite3_context *context, int argc, sqlite3_value **argv) {
    int len;
    int int_value;
    size_t sid;
    double lon;
    double lat;
    int level;
    
    if (sqlite3_value_type(argv[0]) == SQLITE_FLOAT) {
        lon = sqlite3_value_double(argv[0]);
    } else if (sqlite3_value_type(argv[0]) == SQLITE_INTEGER) {
        int_value = sqlite3_value_int(argv[0]);
        lon = int_value;
    } else {
        sqlite3_result_null (context);
        return;
    }
    
    if (sqlite3_value_type(argv[1]) == SQLITE_FLOAT) {
        lat = sqlite3_value_double(argv[1]);
    } else if (sqlite3_value_type(argv[1]) == SQLITE_INTEGER) {
        int_value = sqlite3_value_int (argv[1]);
        lat = int_value;
    } else {
        sqlite3_result_null (context);
        return;
    }
    
    if (sqlite3_value_type(argv[2]) == SQLITE_INTEGER) {
        int_value = sqlite3_value_int (argv[2]);
        level = int_value;  
    } else {
        sqlite3_result_null (context);
        return;
    }
    
    sid = stare.ValueFromLatLonDegrees(lat, lon, 15);
    if (!sid) {
        sqlite3_result_null(context);
    } else {
        sqlite3_result_int64(context, sid);
    }        
}


/*
** Implementation of stare_from_point() function.
** Convert a gaia WKB point to a STARE index
*/


static void stare_from_point(sqlite3_context *context, int argc, sqlite3_value **argv) {
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo = NULL;
    gaiaPointPtr point;
    int gpkg_amphibious = 0;
    int gpkg_mode = 0;
    
    int level;
    size_t sid;
    
    if (sqlite3_value_type(argv[0]) != SQLITE_BLOB) {
	  sqlite3_result_null(context);
	  return;
    }
    
    if (sqlite3_value_type(argv[1]) == SQLITE_INTEGER) {
        level = sqlite3_value_int (argv[1]);
    } else {
        sqlite3_result_null (context);
        return;
    }
    
    p_blob = (unsigned char *) sqlite3_value_blob(argv[0]);
    n_bytes = sqlite3_value_bytes(argv[0]);
    geo = gaiaFromSpatiaLiteBlobWkbEx(p_blob, n_bytes, gpkg_mode, gpkg_amphibious);
    
    if (!geo) {
        sqlite3_result_null(context);
    } else {
        point = geo->FirstPoint;
        if (!point) {
            sqlite3_result_null(context); 
        } else {
            sid = stare.ValueFromLatLonDegrees(point->Y, point->X, 15);
            sqlite3_result_int64(context, sid);
        }
    }
}

 
/*
** Extract the nodes from a Gaia Polygon
*/



LatLonDegrees64ValueVector polygon2points(gaiaGeomCollPtr geo_collection) {
    LatLonDegrees64ValueVector points;
    double x;
    double y;
    
    gaiaPolygonPtr polygon = NULL;
    gaiaRingPtr ring = NULL;
    
    int i = 0;
    if (!geo_collection) {
        return points;
    } else {
        polygon = geo_collection->FirstPolygon;
        while (polygon) {        
            ring = polygon->Exterior;
            if (!ring) {
                return points;
            } else {
                for (int iv = 0; iv < ring->Points; iv++) {                
                    gaiaGetPoint(ring->Coords, iv, &x, &y);
                    points.push_back(LatLonDegrees64(y, x));    
                }
            }       
            polygon->Next;
            if (i>300) {                
                cout << i << endl << flush;
                break;    
            }            
             i++;
            //cout << polygon << flush;
        }           
    }    
    return points;}


/*
** Implementation of stare_from_polygon() function.
** Convert a gaia WKB polygon to a STARE index
*/


static void stare_from_polygon(sqlite3_context *context, int argc, sqlite3_value **argv) {
    unsigned char *p_blob;
    int n_bytes;
    gaiaGeomCollPtr geo_collection = NULL;    
    int gpkg_amphibious = 0;
    int gpkg_mode = 0;
    
    int resolution;
    
    if (sqlite3_value_type(argv[0]) != SQLITE_BLOB) {
	  sqlite3_result_null(context);
	  return;
    }
    
    if (sqlite3_value_type(argv[1]) == SQLITE_INTEGER) {
        resolution = sqlite3_value_int (argv[1]);
    } else {
        sqlite3_result_null (context);
        return;
    }
    
    p_blob = (unsigned char *) sqlite3_value_blob (argv[0]);
    n_bytes = sqlite3_value_bytes(argv[0]);
    geo_collection = gaiaFromSpatiaLiteBlobWkbEx(p_blob, n_bytes, gpkg_mode, gpkg_amphibious);

    LatLonDegrees64ValueVector points = polygon2points(geo_collection);
    
    STARE_SpatialIntervals sis = stare.NonConvexHull(points, resolution);    
    STARE_ArrayIndexSpatialValues sisvs = expandIntervals(sis);

    int size = sisvs.size() * sizeof(sisvs[0]);
    int64_t array[sisvs.size()];
    int i = 0;
    for (int64_t val: sisvs){
        array[i] = val;
        i ++;
    }
    sqlite3_result_blob64(context, &array, size, SQLITE_TRANSIENT);        
}


/*
** Implementation of decode() and encode() function.
** Converts a stareblob to a string representation and a string representation to a stareblob
*/



STARE_ArrayIndexSpatialValues blob2sisvs(unsigned char *p_blob, int n_bytes) {
    STARE_ArrayIndexSpatialValues sisvs;    
    int size = sizeof(int64_t);
    for (int i=0; i<n_bytes; i+=size) {        
        sisvs.push_back(gaiaImportI64(p_blob + i, 1, 1));
    }
    return sisvs;
}


static void decode_stareblob(sqlite3_context *context, int argc, sqlite3_value **argv) {
    unsigned char *p_blob;
    int n_bytes;
    
    if (sqlite3_value_type(argv[0]) != SQLITE_BLOB) {
	  sqlite3_result_null(context);
	  return;
    }
        
    p_blob = (unsigned char *) sqlite3_value_blob(argv[0]);
    n_bytes = sqlite3_value_bytes(argv[0]);
    STARE_ArrayIndexSpatialValues sisvs = blob2sisvs(p_blob, n_bytes);
    
    std::string str;
    str += "[";
    for (size_t value: sisvs) {
        str += std::to_string(value);        
        str += ", ";
    }
    str += "]";
        
    char result_string[str.length()]; 
    strcpy(result_string, str.c_str());
    
    sqlite3_result_text(context, result_string, 500, SQLITE_TRANSIENT);//Why 500? should it be str.length()?
}

void removeChar(char *str, char garbage) {

    char *src, *dst;
    for (src = dst = str; *src != '\0'; src++) {
        *dst = *src;
        if (*dst != garbage) dst++;
    }
    *dst = '\0';
}


static void encode_stareblob(sqlite3_context *context, int argc, sqlite3_value **argv) {
    
    unsigned char *p_blob;
    int n_bytes;
    
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT) {
	  sqlite3_result_null(context);
	  return;
    }
    
    char *string = reinterpret_cast<char*>(const_cast<unsigned char*>(sqlite3_value_text(argv[0])));
    removeChar(string, '[');
    removeChar(string, ']');
    removeChar(string, ' ');

    char * token = strtok(string, ",");
    char* pEnd;
    std::vector<int64_t> sisvs;
    while( token != NULL ) {
        sisvs.push_back(strtoll(token, &pEnd, 10));
        token = strtok(NULL, "\n");
    }
    
    // Convert vector to array
    int size = sisvs.size() * sizeof(sisvs[0]);
    int64_t array[sisvs.size()];
    int i = 0;
    for (int64_t val: sisvs){
        array[i] = val;
        i ++;
    }
    sqlite3_result_blob64(context, &array, size, SQLITE_TRANSIENT);      
}

static void encode_float_blob(sqlite3_context *context, int argc, sqlite3_value **argv) {
    
    if (sqlite3_value_type(argv[0]) != SQLITE_TEXT) {
	  sqlite3_result_null(context);
	  return;
    }
    
    char *string = reinterpret_cast<char*>(const_cast<unsigned char*>(sqlite3_value_text(argv[0])));
    removeChar(string, '[');
    removeChar(string, ']');
    removeChar(string, ' ');

    char * token = strtok(string, ",");
    char* pEnd;
    std::vector<double> dbls;
    while( token != NULL ) {
        dbls.push_back(strtod(token, &pEnd));
        token = strtok(NULL, "\n");
    }
    
    // Convert vector to array
    int size = dbls.size() * sizeof(dbls[0]);
    double array[dbls.size()];
    int i = 0;
    for (double val: dbls){
        array[i] = val;
        i ++;
    }
    sqlite3_result_blob64(context, &array, size, SQLITE_TRANSIENT);      
}

std::vector<double> blob2doubles(unsigned char *p_blob, int n_bytes) {
    std::vector<double> dbls;    
    int size = sizeof(double);
    for (int i=0; i<n_bytes; i+=size) {        
        dbls.push_back(gaiaImport64(p_blob + i, 1, 1));
    }
    return dbls;
}

static void decode_float_blob(sqlite3_context *context, int argc, sqlite3_value **argv) {
    unsigned char *p_blob;
    int n_bytes;
    
    if (sqlite3_value_type(argv[0]) != SQLITE_BLOB) {
	  sqlite3_result_null(context);
	  return;
    }
        
    p_blob = (unsigned char *) sqlite3_value_blob(argv[0]);
    n_bytes = sqlite3_value_bytes(argv[0]);
    std::vector<double> dbls = blob2doubles(p_blob, n_bytes);
    
    std::string str;
    str += "[";
    for (double value: dbls) {
        str += std::to_string(value);        
        str += ", ";
    }
    str += "]";
        
    char result_string[str.length()]; 
    strcpy(result_string, str.c_str());
    
    sqlite3_result_text(context, result_string, str.length(), SQLITE_TRANSIENT);
}

/*https://www.sqlite.org/appfunc.html
Example: 
typedef struct CountCtx CountCtx;
struct CountCtx {
  i64 n;
};
static void countStep(sqlite3_context *context, int argc, sqlite3_value **argv){
  CountCtx *p;
  p = sqlite3_aggregate_context(context, sizeof(*p));
  if( (argc==0 || SQLITE_NULL!=sqlite3_value_type(argv[0])) && p ){
    p->n++;
  }
}   
static void countFinalize(sqlite3_context *context){
  CountCtx *p;
  p = sqlite3_aggregate_context(context, 0);
  sqlite3_result_int64(context, p ? p->n : 0);
}
*/
/* An Aggregate function that is used to sum up all ellements of all BLOB array tuples */
typedef struct Sum_BLOB_Array Sum_BLOB_Array; 
struct Sum_BLOB_Array {
    double sum;
};
static void sum_blob_array_Step(sqlite3_context *context, int argc, sqlite3_value **argv){
    Sum_BLOB_Array *p;
    p = (Sum_BLOB_Array*)sqlite3_aggregate_context(context, sizeof(*p));

    unsigned char *p_blob;
    int n_bytes;
    
    if (sqlite3_value_type(argv[0]) != SQLITE_BLOB) {
	  sqlite3_result_null(context);
	  return;
    }
        
    p_blob = (unsigned char *) sqlite3_value_blob(argv[0]);
    n_bytes = sqlite3_value_bytes(argv[0]);
    std::vector<double> dbls = blob2doubles(p_blob, n_bytes);

    for (double value: dbls) {
        p->sum += value;
    }
}

static void sum_blob_array_Final(sqlite3_context *context){
    Sum_BLOB_Array *p;
    p = (Sum_BLOB_Array*)sqlite3_aggregate_context(context, sizeof(*p));
    std:string str_sum = std::to_string(p->sum);

    char result_string[str_sum.length()]; 
    strcpy(result_string, str_sum.c_str());

    sqlite3_result_text(context, result_string, str_sum.length(), SQLITE_TRANSIENT);
    //sqlite3_result_double(context, p->sum); if a double is expected
}

static void sum_blob_all_element_array(sqlite3_context *context, int argc, sqlite3_value **argv){
    unsigned char *p_blob;
    int n_bytes;
    
    if (sqlite3_value_type(argv[0]) != SQLITE_BLOB) {
	  sqlite3_result_null(context);
	  return;
    }
        
    p_blob = (unsigned char *) sqlite3_value_blob(argv[0]);
    n_bytes = sqlite3_value_bytes(argv[0]);
    std::vector<double> dbls = blob2doubles(p_blob, n_bytes);
    
    double dbl_sum = 0.0;
    for (double value: dbls) {
        dbl_sum += value;
    }
    std::string str_sum = std::to_string(dbl_sum);

    char result_string[str_sum.length()]; 
    strcpy(result_string, str_sum.c_str());
    
    sqlite3_result_text(context, result_string, str_sum.length(), SQLITE_TRANSIENT);
    //sqlite3_result_double(context, dbl_sum);//If we want to return a double instead of string
}

/*
** Implementation of stare_intersects() with helper functions brute_intersects and sorted_intersects().
** Returns true/false if two stare index value ranges intersect
*/


int brute_intersects(int64_t * indices1, int len1, int64_t * indices2, int len2){
    int intersects = 0;    

    for(int i=0; i<len2; ++i) {     
        for(int j=0; j<len1; ++j) {
            intersects = cmpSpatial(indices2[i], indices1[j]);            
            if (intersects != 0) break;
        }
        if (intersects != 0) break;
    }
    
    return intersects;
}

int sorted_intersects(int64_t * indices1, int len1, int64_t * indices2, int len2){
    int intersects;
    sort(indices1, indices1+len1);
    
    intersects = 0;
    for(int i=0; i<len2; ++i) {
        STARE_ArrayIndexSpatialValue test_siv = indices2[i];        
        int start = 0;
        int end = len1-1;
        int m = (start+end)/2;
        bool done = false;
        while( !done ) {
            m = (start+end)/2;
            if(indices1[m] < test_siv) {
                start = m+1;
                done = start > end;
            } else if(indices1[m] > test_siv) {
                end = m-1;
                done = start > end;
            } else {
                intersects = 1; 
                done = true;
            }
        }
        if( intersects == 0 ) {
            if( (end >= 0) || (start < len1) ) {
                if( 0 <= m-1 ) {
                    if( cmpSpatial(indices1[m-1],test_siv) != 0 ) {
                        intersects = 1;
                        break;
                    }
                }
                if( (0 <= m) && (m < len1) ) {
                    if( cmpSpatial(indices1[m],test_siv) != 0 ) {
                        intersects = 1;
                        break;
                    }
                }
                if( m+1 < len1 ) {
                    if( cmpSpatial(indices1[m+1],test_siv) != 0 ) {
                        intersects = 1;
                        break;
                    }
                }
            }
        }
    }   
    return intersects;
}

static void stare_intersects(sqlite3_context *context, int argc, sqlite3_value **argv) {
    unsigned char *p_blob;
    int n_bytes;
    STARE_ArrayIndexSpatialValues sisvs_1;
    STARE_ArrayIndexSpatialValues sisvs_2;
    
    if (sqlite3_value_type(argv[0]) == SQLITE_INTEGER) {        
        sisvs_1.push_back(sqlite3_value_int64(argv[0])); 
    } else if (sqlite3_value_type(argv[0]) == SQLITE_BLOB) {
        p_blob = (unsigned char *) sqlite3_value_blob(argv[0]);
        n_bytes = sqlite3_value_bytes(argv[0]);
        sisvs_1 = blob2sisvs(p_blob, n_bytes);
    } else {       
        sqlite3_result_null (context);
        return;
    }
    
    if (sqlite3_value_type(argv[1]) == SQLITE_INTEGER) {        
        sisvs_2.push_back(sqlite3_value_int64(argv[1])); 
    } else if (sqlite3_value_type(argv[1]) == SQLITE_BLOB) {
        p_blob = (unsigned char *) sqlite3_value_blob(argv[1]);
        n_bytes = sqlite3_value_bytes(argv[1]);
        sisvs_2 = blob2sisvs(p_blob, n_bytes);        
    } else {       
        sqlite3_result_null (context);
        return;
    }
    
    int n_1= sisvs_1.size();
    int n_2= sisvs_2.size();
    int64_t index_values_1[n_1];
    int64_t index_values_2[n_2];
    int k;
    
    k = 0;
    for (int64_t value: sisvs_1) {        
        index_values_1[k] = value;       
        k ++;
    }   
    
    k = 0;
    for (int64_t value: sisvs_2) {        
        index_values_2[k] = value;
        k ++;
    }   
    int intersects = sorted_intersects(index_values_1, n_1, index_values_2, n_2);
    sqlite3_result_int(context, intersects);
}





// Regarding "extern C":
// https://stackoverflow.com/questions/28856061/how-to-statically-link-sqlite3-extension-functions-into-a-c-c-application
extern "C" {
    int sqlite3_starelite_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi) {
        int rc = SQLITE_OK;
        SQLITE_EXTENSION_INIT2(pApi);
        (void)pzErrMsg;  /* Unused parameter */
        rc = sqlite3_create_function(db, "stare_from_lonlat", 3,
                                     SQLITE_UTF8|SQLITE_DETERMINISTIC,
                                    0, stare_from_lonlat, 0, 0);        
        rc = sqlite3_create_function(db, "stare_from_point", 2,
                                     SQLITE_UTF8|SQLITE_DETERMINISTIC,
                                     0, stare_from_point, 0, 0);
        rc = sqlite3_create_function(db, "stare_from_polygon", 2,
                                     SQLITE_UTF8|SQLITE_DETERMINISTIC,
                                     0, stare_from_polygon, 0, 0);
        rc = sqlite3_create_function(db, "decode_stareblob", 1,
                                     SQLITE_UTF8|SQLITE_DETERMINISTIC,
                                     0, decode_stareblob, 0, 0);
        rc = sqlite3_create_function(db, "encode_stareblob", 1,
                                     SQLITE_UTF8|SQLITE_DETERMINISTIC,
                                     0, encode_stareblob, 0, 0);
        rc = sqlite3_create_function(db, "decode_float_blob", 1,
                                     SQLITE_UTF8|SQLITE_DETERMINISTIC,
                                     0, decode_float_blob, 0, 0);
        rc = sqlite3_create_function(db, "encode_float_blob", 1,
                                     SQLITE_UTF8|SQLITE_DETERMINISTIC,
                                     0, encode_float_blob, 0, 0);
        rc = sqlite3_create_function(db, "sum_blob_all_element_array", 1,
                                     SQLITE_UTF8|SQLITE_DETERMINISTIC,
                                     0, sum_blob_all_element_array, 0, 0);
        rc = sqlite3_create_function(db, "stare_intersects", 2,
                                     SQLITE_UTF8|SQLITE_DETERMINISTIC,
                                     0, stare_intersects, 0, 0);
        /* Aggregate function */
        rc = sqlite3_create_function(db, "sum_blob_array", 1,
                                     SQLITE_UTF8|SQLITE_DETERMINISTIC,
                                     0, 0, sum_blob_array_Step, sum_blob_array_Final);
/*
https://www.sqlite.org/capi3.html:
   typedef struct sqlite3_value sqlite3_value;
   int sqlite3_create_function(
     sqlite3 *,
     const char *zFunctionName,
     int nArg,
     int eTextRep,
     void*,
     void (*xFunc)(sqlite3_context*,int,sqlite3_value**),
     void (*xStep)(sqlite3_context*,int,sqlite3_value**),
     void (*xFinal)(sqlite3_context*)
   );
   int sqlite3_create_function16(
     sqlite3*,
     const void *zFunctionName,
     int nArg,
     int eTextRep,
     void*,
     void (*xFunc)(sqlite3_context*,int,sqlite3_value**),
     void (*xStep)(sqlite3_context*,int,sqlite3_value**),
     void (*xFinal)(sqlite3_context*)
   );
   #define SQLITE_UTF8     1
   #define SQLITE_UTF16    2
   #define SQLITE_UTF16BE  3
   #define SQLITE_UTF16LE  4
   #define SQLITE_ANY      5
*/                
        return rc;
    }
}
