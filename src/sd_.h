#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <SPI.h>
#include <FS.h>
#include "SD.h"

#define USER_DB "/sd/Database/User.db"
class User_if{
    public:
    const char *name;
    int finger_id ;
};
static int callback(void *data, int argc, char **argv, char **azColName);
int openDb(const char *filename, sqlite3 **db);
int db_exec(sqlite3 *db, const char *sql);
int db_query(int data,User_if *user);
int db_insert(char *id,char *name,char *role);
int db_delete(char *data,User_if *user);