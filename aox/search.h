// Copyright Oryx Mail Systems GmbH. All enquiries to info@oryx.com, please.

#ifndef SEARCH_H
#define SEARCH_H

#include "aoxcommand.h"


void dumpSelector( class Selector *, uint l = 0 );


class ShowSearch
    : public AoxCommand
{
public:
    ShowSearch( EStringList * );

    void execute();
};


#endif
