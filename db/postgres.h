// Copyright Oryx Mail Systems GmbH. All enquiries to info@oryx.com, please.

#ifndef POSTGRES_H
#define POSTGRES_H

#include "database.h"

class Row;


class Postgres
    : public Database
{
public:
    Postgres();
    ~Postgres();

    bool ready();
    void enqueue( class Query * );
    void execute();

    void react( Event e );

private:
    class PgData *d;

    void authentication( char );
    void backendStartup( char );
    void process( char );
    void unknown( char );
    void error( const String & );

    bool haveMessage();
    void processQueue( bool = false );
    void updateSchema();
};


#endif
