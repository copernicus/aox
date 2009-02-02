// Copyright Oryx Mail Systems GmbH. All enquiries to info@oryx.com, please.

#include "undelete.h"

#include "searchsyntax.h"
#include "transaction.h"
#include "integerset.h"
#include "selector.h"
#include "mailbox.h"
#include "query.h"
#include "map.h"
#include "utf.h"

#include <stdlib.h> // exit()
#include <stdio.h> // printf()


class UndeleteData
    : public Garbage
{
public:
    UndeleteData(): state( 0 ), m( 0 ), t( 0 ),
                    find( 0 ), uidnext( 0 ), usernames( 0 ) {}

    uint state;
    Mailbox * m;
    Transaction * t;

    Query * find;
    Query * uidnext;
    Query * usernames;
};


/*! \class Undelete Undelete.h
    This class handles the "aox undelete" command.
*/

Undelete::Undelete( StringList * args )
    : AoxCommand( args ), d( new UndeleteData )
{
}


void Undelete::execute()
{
    if ( d->state == 0 ) {
        database( true );
        Mailbox::setup();
        d->state = 1;
        parseOptions();
    }

    if ( d->state == 1 ) {
        if ( !choresDone() )
            return;
        d->state = 2;
    }

    if ( d->state == 2 ) {
        Utf8Codec c;
        UString m = c.toUnicode( next() );

        if ( !c.valid() )
            error( "Encoding error in mailbox name: " + c.error() );
        else if ( m.isEmpty() )
            error( "No mailbox name" );
        else
            d->m = Mailbox::find( m, true );
        if ( !d->m )
            error( "No such mailbox: " + m.utf8() );

        Selector * s = parseSelector( args() );
        if ( !s )
            exit( 1 );
        s->simplify();

        d->t = new Transaction( this );
        if ( d->m->deleted() ) {
            if ( !d->m->create( d->t, 0 ) )
                error( "Mailbox was deleted; recreating failed: " +
                       d->m->name().utf8() );
            printf( "aox: Note: Mailbox %s is recreated.\n"
                    "     Its ownership and permissions could not be restored.\n",
                    d->m->name().utf8().cstr() );
        }

        StringList wanted;
        wanted.append( "uid" );
        if ( opt( 'v' ) ) {
            wanted.append( "deleted_by" );
            wanted.append( "deleted_at::text" );
            wanted.append( "reason" );
            d->usernames = new Query( "select id, login from users", 0 );
            d->t->enqueue( d->usernames );
        }

        d->find = s->query( 0, d->m, 0, 0, true, &wanted, true );
        d->t->enqueue( d->find );

        d->uidnext = new Query( "select uidnext, nextmodseq "
                                "from mailboxes "
                                "where id=$1 for update", this );
        d->uidnext->bind( 1, d->m->id() );
        d->t->enqueue( d->uidnext );

        d->t->execute();
        d->state = 3;
    }

    if ( d->state == 3 ) {
        if ( !d->uidnext->done() )
            return;

        Row * r = d->uidnext->nextRow();
        if ( !r )
            error( "Internal error - could not read mailbox UID" );
        uint uidnext = r->getInt( "uidnext" );
        int64 modseq = r->getBigint( "nextmodseq" );

        Map<String> logins;
        if ( d->usernames ) {
            while ( d->usernames->hasResults() ) {
                r = d->usernames->nextRow();
                logins.insert( r->getInt( "id" ),
                               new String( r->getString( "login" ) ) );
            }
        }

        Map<String> why;
        IntegerSet s;
        while ( d->find->hasResults() ) {
            r = d->find->nextRow();
            uint uid = r->getInt( "uid" );
            s.add( uid );
            if ( d->usernames )
                why.insert( uid,
                            new String( 
                                " - Message " + fn( uid ) + " was deleted by " +
                                (*logins.find( r->getInt( "deleted_by" ) )).quoted() +
                                " at " + r->getString( "deleted_at" ) +
                                "\n   Reason: " +
                                r->getString( "reason" ).simplified().quoted() ) );
        }

        if ( s.isEmpty() )
            error( "No such deleted message (search returned 0 results)" );

        printf( "aox: Undeleting %d messages into %s\n",
                s.count(), d->m->name().utf8().cstr() );

        Map<String>::Iterator i( why );
        while ( i ) {
            printf( "%s\n", i->cstr() );
            ++i;
        }

        Query * q;

        q = new Query( "create temporary sequence s start " + fn( uidnext ),
                       0 );
        d->t->enqueue( q );

        q = new Query( "insert into mailbox_messages "
                       "(mailbox,uid,message,modseq) "
                       "select $1,nextval('s'),message,$2 "
                       "from deleted_messages "
                       "where mailbox=$1 and uid=any($3)", 0 );
        q->bind( 1, d->m->id() );
        q->bind( 2, modseq );
        q->bind( 3, s );
        d->t->enqueue( q );

        q = new Query( "delete from deleted_messages "
                       "where mailbox=$1 and uid=any($2)", 0 );
        q->bind( 1, d->m->id() );
        q->bind( 5, s );
        d->t->enqueue( q );

        q = new Query( "update mailboxes "
                       "set uidnext=nextval('s'), nextmodseq=$1 "
                       "where id=$2", 0 );
        q->bind( 1, modseq + 1 );
        q->bind( 2, d->m->id() );
        d->t->enqueue( q );

        d->t->enqueue( new Query( "drop sequence s", 0 ) );

        Mailbox::refreshMailboxes( d->t );

        d->t->commit();
        d->state = 4;
    }

    if ( d->state == 4 ) {
        if ( !d->t->done() )
            return;

        if ( d->t->failed() )
            error( "Undelete failed." );
        finish();
    }
}
