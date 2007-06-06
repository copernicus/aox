// Copyright Oryx Mail Systems GmbH. All enquiries to info@oryx.com, please.

#ifndef PAGECOMPONENT_H
#define PAGECOMPONENT_H

#include "event.h"
#include "string.h"
#include "list.h"


class UString;
class FrontMatter;
class WebPage;


class PageComponent
    : public EventHandler
{
public:
    PageComponent( const String & );

    bool done() const;
    void execute();

    uint status() const;
    void setStatus( uint );

    WebPage * page() const;
    void setPage( WebPage * );

    String contents() const;
    void setContents( const String & );

    List<FrontMatter> * frontMatter() const;
    void addFrontMatter( FrontMatter * );

    List<PageComponent> * subComponents() const;
    void addSubComponent( PageComponent * );

    String divClass() const;

    static String quoted( const String & );
    static String address( class Address * );
    static String address( const UString & );

    uint uniqueNumber();

private:
    class PageComponentData * d;
};


#endif
