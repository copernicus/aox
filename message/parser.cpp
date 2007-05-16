// Copyright Oryx Mail Systems GmbH. All enquiries to info@oryx.com, please.

#include "parser.h"

#include "ustring.h"
#include "codec.h"
#include "utf.h"


/*! \class Parser822 parser.h

    The Parser822 class provides parser help for RFC 822-like grammars.
    It properly is more like a lexer than a parser, but also not quite
    like a lexer.

    Parser822 provides a cursor, and member functions to read many
    RFC 2822 productions at the cursor. Generally, each member returns
    the production read or an empty string.
*/


/*! \fn Parser822::Parser822( const String & s )
    Creates a new RFC 822 parser object to parse \a s.
*/


/*! Returns true if \a c belongs to the RFC 2822 'atext' production, and
    false in all other circumstances.
*/

bool Parser822::isAtext( char c ) const
{
    if ( c < 32 || c > 127 )
        return false;

    if ( ( c >= 'a' && c <= 'z' ) ||
         ( c >= 'A' && c <= 'Z' ) ||
         ( c >= '0' && c <= '9' ) )
        return true;

    switch ( c ) {
    case '!':
    case '#':
    case '$':
    case '%':
    case '&':
    case '\'':
    case '*':
    case '+':
    case '-':
    case '/':
    case '=':
    case '?':
    case '^':
    case '_':
    case '`':
    case '{':
    case '|':
    case '}':
    case '~':
        return true;
        break;
    default:
        break;
    }

    return false;
}


/*! Asserts that the index points at \a expect and steps past it. If the
    index doesn't, \a errorMessage is logged.

    If \a expect has length 0, this function is a noop.
*/

void Parser822::stepPast( const char * expect, const char * errorMessage )
{
    if ( !expect || !*expect )
        return;
    int j = 0;
    while ( expect[j] != 0 && s[i+j] == expect[j] )
        j++;
    if ( expect[j] )
        error( errorMessage );
    else
        i = i + j;
}


/*! Moves index() to the first nonwhitespace character after the current
    point. If index() points to nonwhitespace already, it is not moved.
*/

void Parser822::whitespace()
{
    while ( i < s.length() &&
            ( s[i] == ' ' || s[i] == 9 || s[i] == 10 || s[i] == 13 ||
              s[i] == 160 ) )
        i++;
}


/*! \fn uint Parser822::index() const
    Returns the current position of the parser cursor. This is 0 at the
    start, and advances as characters are parsed.
*/


/*! Moves index() past all comments and surrounding white space, and
    returns the contents of the last comment.

    Returns a null string if there was no comment.
*/

String Parser822::comment()
{
    String r;
    whitespace();
    while ( s[i] == '(' ) {
        r = "";
        uint commentLevel = 0;
        do {
            switch( s[i] ) {
            case '(':
                if ( commentLevel > 0 )
                    r.append( '(' );
                commentLevel++;
                break;
            case ')':
                commentLevel--;
                if ( commentLevel > 0 )
                    r.append( ')' );
                break;
            case '\\':
                r.append( s[++i] );
                break;
            default:
                r.append( s[i] );
                break;
            }
            i++;
        } while( commentLevel && i < s.length() );
        whitespace();
    }
    return r;
}


/*! Steps past an atom or a quoted-text, and returns that text. */

String Parser822::string()
{
    comment();

    // now, treat it either as a quoted string or an unquoted atom
    if ( s[i] != '"' )
        return atom();

    String output;
    i++;
    bool done = false;
    while( !done && i < s.length() ) {
        if ( s[i] == '"' ) {
            i++;
            done = true;
        }
        else if ( s[i] == '\\' ) {
            output.append( s[++i] );
            i++;
        }
        else if ( s[i] == 9 || s[i] == 10 || s[i] == 13 || s[i] == ' ' ) {
            uint wsp = i;
            whitespace();
            String t( s.mid( wsp, i-wsp ) );
            if ( t.contains( "\r" ) || t.contains( "\n" ) )
                output.append( ' ' );
            else
                output.append( t );
        }
        else {
            output.append( s[i++] );
        }
    }
    return output;
}


/*! Returns a single character and steps to the next. */

char Parser822::character()
{
    return s[i++];
}


/*! Returns a single domain and steps past it.

    This isn't correct at the moment, but I think it will eventually be...

    Note that our definition of domain differs from the RFC 822 one. We
    only accept three forms: Something that may be a DNS A record,
    something that may be an IPv4 address in '[]' and something that may
    be an IPv6 address, again in '[]'. Examples: 'lupinella.troll.no',
    '[213.203.59.59]' and '[IPv6:::ffff:213.203.59.59]'.
*/

String Parser822::domain()
{
    String l;
    comment();
    if ( next() == '[' ) {
        int j = i;
        step();
        char c = s[i];
        while ( ( c >= 'a' && c <= 'z' ) ||
                ( c >= 'A' && c <= 'Z' ) ||
                ( c >= '0' && c <= '9' ) ||
                c == '.' || c == ':' || c == '-' ) {
            step();
            c = next();
        }
        if ( next() == ']' )
            step();
        else
            error( "missing trailing ']' ([1.2.3.4])" );
        l = s.mid( j, i-j );
    }
    else {
        l = dotAtom();
    }
    return l;
}


/*! Sets this Parser822 object to parse MIME strings if \a m is true,
    and RFC 2822 strings if \a m is false. The only difference is the
    definition of specials.
*/

void Parser822::setMime( bool m )
{
    mime = m;
}


/*! Returns a dot-atom, stepping past all relevant whitespace and
    comments.
*/

String Parser822::dotAtom()
{
    String r = atom();
    if ( r.isEmpty() )
        return r;

    bool m = true;
    comment();
    while ( m && s[i] == '.' ) {
        int j = i;
        i++;
        String a = atom();
        if ( a.isEmpty() ) {
            i = j; // backtrack to the dot
            m = false;
        }
        else {
            r = r + "." + a;
            comment();
        }
    }

    return r;
}


/*! Returns a single atom, stepping past white space and comments
    before and after it.
*/

String Parser822::atom()
{
    comment();
    String output;
    while ( i < s.length() && isAtext( s[i] ) )
        output.append( s[i++] );
    return output;
}


/*! Returns a single MIME token (as defined in RFC 2045 section 5), which
    is an atom minus [/?=] plus [.].
*/

String Parser822::mimeToken()
{
    comment();

    String output;
    char c = s[i];

    while ( i < s.length() &&
            c > 32 && c < 128 &&
            c != '(' && c != ')' && c != '<' && c != '>' &&
            c != '@' && c != ',' && c != ';' && c != ':' &&
            c != '[' && c != ']' && c != '?' && c != '=' &&
            c != '\\' && c != '"' && c != '/' )
    {
        output.append( c );
        i++;
        c = s[i];
    }

    return output;
}


/*! Returns a single MIME value (as defined in RFC 2045 section 5), which
    is an atom minus [/?=] plus [.] (i.e., a MIME token) or a quoted
    string.
*/

String Parser822::mimeValue()
{
    comment();
    if ( s[i] == '"' )
        return string();
    return mimeToken();
}


/*! Steps past a MIME encoded-word (as defined in RFC 2047) and returns
    its decoded UTF-8 representation, or an empty string if the cursor
    does not point to a valid encoded-word. The caller is responsible
    for checking that the encoded-word is separated from neighbouring
    tokens by whitespace.

    The characters permitted in the encoded-text are adjusted based on
    \a type, which may be Text (by default), Comment, or Phrase.
*/

String Parser822::encodedWord( EncodedText type )
{
    String out;

    // encoded-word = "=?" charset '?' encoding '?' encoded-text "?="

    int n = i;
    Codec *cs = 0;
    bool valid = true;
    String charset, text;
    char encoding = 0;

    if ( s[n] != '=' || s[++n] != '?' )
        valid = false;

    if ( valid ) {
        int m = ++n;
        char c = s[m];
        while ( m - i <= 75 &&
                c > 32 && c < 128 &&
                c != '(' && c != ')' && c != '<' && c != '>' &&
                c != '@' && c != ',' && c != ';' && c != ':' &&
                c != '[' && c != ']' && c != '?' && c != '=' &&
                c != '\\' && c != '"' && c != '/' && c != '.' )
        {
            charset.append( c );
            c = s[++m];
        }

        // XXX: Should we treat unknown charsets as us-ascii?
        int j = charset.find( '*' );
        if ( j > 0 ) {
            // XXX: What should we do with the language information?
            charset = charset.mid( 0, j );
        }

        if ( m - i > 75 || ( cs = Codec::byName( charset ) ) == 0 )
            valid = false;
        else
            n = m;
    }

    if ( valid && s[n] != '?' )
        valid = false;

    if ( valid ) {
        int m = ++n;
        encoding = s[m] | 0x20;
        if ( encoding != 'q' && encoding != 'b' )
            valid = false;
        else
            n = ++m;
    }

    if ( valid && s[n] != '?' )
        valid = false;

    if ( valid ) {
        int m = ++n;
        char c = s[m];

        if ( encoding == 'b' ) {
            while ( m - i <= 75 &&
                    ( ( c >= '0' && c <= '9' ) ||
                      ( c >= 'a' && c <= 'z' ) ||
                      ( c >= 'A' && c <= 'Z' ) ||
                      c == '+' || c == '/' || c == '=' ) )
            {
                text.append( c );
                c = s[++m];
            }
        }
        else {
            while ( m - i <= 75 &&
                    c > 32 && c < 128 && c != '?' &&
                    ( type != Comment ||
                      ( c != '(' && c != ')' && c != '\\' ) ) &&
                    ( type != Phrase ||
                      ( c >= '0' && c <= '9' ) ||
                      ( c >= 'a' && c <= 'z' ) ||
                      ( c >= 'A' && c <= 'Z' ) ||
                      ( c == '!' || c == '*' || c == '-' ||
                        c == '/' || c == '=' || c == '_' ||
                        c == '\'' ) ) )
            {
                text.append( c );
                c = s[++m];
            }
        }

        if ( m - i > 75 )
            valid = false;
        else
            n = m;
    }

    if ( valid && ( s[n] != '?' || s[++n] != '=' ) )
        valid = false;

    if ( valid ) {
        Utf8Codec u;
        if ( encoding == 'q' )
            text = text.deQP( true );
        else
            text = text.de64();
        out = u.fromUnicode( cs->toUnicode( text ) );
        i = ++n;
    }

    return out;
}


/*! Do RFC 2047 decoding of \a s, totally ignoring what the
    encoded-text in \a s might be.
    
    Depending on circumstances, the encoded-text may contain different
    sets of characters. Moreover, not every 2047 encoder obeys the
    rules. This function checks nothing, it just decodes.
*/

UString Parser822::de2047( const String & s )
{
    UString out;

    if ( !s.startsWith( "=?" ) || !s.endsWith( "?=" ) )
        return out;
    int cs = 2;
    int ce = s.find( '*', 2 );
    int es = s.find( '?', 2 ) + 1;
    if ( es < cs )
        return out;
    if ( ce < cs )
        ce = es;
    if ( ce >= es )
        ce = es-1;
    Codec * codec = Codec::byName( s.mid( cs, ce-cs ) );
    if ( s[es+1] != '?' )
        return out;
    String encoded = s.mid( es+2, s.length() - es - 2 - 2 );
    String decoded;
    switch ( s[es] ) {
    case 'Q':
    case 'q':
        decoded = encoded.deQP( true );
        break;
    case 'B':
    case 'b':
        decoded = encoded.de64();
        break;
    default:
        return out;
        break;
    }

    if ( !codec ) {
        // if we didn't recognise the codec, we'll assume that it's
        // ASCII if that would work and otherwise refuse to decode.
        uint i = 0;
        while ( i < decoded.length() &&
                decoded[i] >= ' ' && decoded[i] < 127 )
            i++;
        if ( i >= decoded.length() )
            codec = new AsciiCodec;
    }

    if ( codec )
        out = codec->toUnicode( decoded );
    return out;
}


/*! Steps past a sequence of adjacent encoded-words with whitespace in
    between and returns the decoded UTF-8 representation.
*/

String Parser822::encodedWords()
{
    String out;

    String us = encodedWord();
    if ( us.isEmpty() )
        return out;

    uint n;
    out.append( us );
    do {
        n = i;
        while ( i < s.length() &&
                ( s[i] == ' ' || s[i] == '\t' ) )
            i++;

        if ( i == n )
            break;

        if ( i < s.length() && s[i] == '=' && s[i+1] == '?' ) {
            String us = encodedWord();
            if ( us.isEmpty() ) {
                i = n;
                break;
            }
            else {
                out.append( us );
            }
        }
        else {
            i = n;
            break;
        }
    }
    while ( 1 );

    return out;
}


/*! Steps past the longest "*text" (a series of text/encoded-words) at
    the cursor and returns its UTF-8 decoded representation, which may
    be an empty string.
*/

String Parser822::text()
{
    String out;

    uint first = i;

    char c = s[i];
    while ( i < s.length() &&
            c != 0 && c != '\012' && c != '\015' && c <= 127 )
    {
        if ( ( c == ' ' && s[i+1] == '=' && s[i+2] == '?' ) ||
             ( i == first && s[i] == '=' && s[i+1] == '?' ) )
        {
            if ( c == ' ' )
                c = s[++i];
            if ( i != first )
                out.append( ' ' );

            uint n = i;
            String us = encodedWords();
            if ( !us.isEmpty() &&
                 ( s[i] == ' ' || s[i] == '\012' || s[i] == '\015' ||
                   i == s.length() ) )
            {
                out.append( us );
                c = s[i];
            }
            else {
                i = n;
                out.append( c );
                c = s[++i];
            }
        }
        else {
            out.append( c );
            c = s[++i];
        }
    }

    return out;
}


/*! Steps past an RFC 822 phrase (a series of word/encoded-words) at the
    cursor and returns its decoded UTF-8 representation, which may be an
    empty string.
*/

String Parser822::phrase()
{
    String out;
    int last = 0;

    i += cfws();
    while ( i < s.length() ) {
        String t;
        int type = 0;

        if ( s[i] == '=' && s[i+1] == '?' ) {
            uint n = i;
            t = encodedWord( Phrase );
            if ( !t.isEmpty() &&
                 ( cfws() > 0 || s[i+1] == '\0' ) )
                type = 1;
            else
                i = n;
        }
        else if ( s[i] == '"' ) {
            t = string();
            type = 2;
        }

        if ( type == 0 )
            t = atom();

        if ( t.isEmpty() )
            break;

        if ( !( out.isEmpty() || ( last == 1 && type == 1 ) ) )
            out.append( ' ' );
        out.append( t );
        last = type;

        uint n = i;
        i += cfws();
        if ( i == n )
            break;
    }

    return out;
}


/*! Returns the number of CFWS characters at the cursor, but does
    nothing else.
*/

int Parser822::cfws()
{
    uint n = 0;
    uint j = i;

    do {
        if ( s[j] == '\040' || s[j] == '\011' ||
             s[j] == '\012' || s[j] == '\015' )
        {
            n++;
            j++;
        }
        else if ( s[j] == '(' ) {
            uint l = 0;
            while ( s[j] == '(' ) {
                uint level = 0;
                do {
                    l++;
                    switch ( s[j] ) {
                    case '(':
                        level++;
                        break;
                    case ')':
                        level--;
                        break;
                    case '\\':
                        j++;
                        l++;
                        break;
                    }
                    j++;
                }
                while ( level != 0 && j < s.length() );
            }
            if ( l == 0 )
                break;
            n += l;
        }
        else {
            break;
        }
    }
    while ( 1 );

    return n;
}


/*! Skips past whitespace, parses a decimal number and returns that
    number.
*/

uint Parser822::number()
{
    comment();
    uint b = i;
    while ( i < s.length() && s[i] >= '0' && s[i] <= '9' )
        i++;
    if ( i == b )
        error( "expected decimal number" );
    bool ok = false;
    uint n = s.mid( b, i-b ).number( &ok );
    if ( !ok ) {
        String e = "number " + s.mid( b, i-b ) + " is bad somehow";
        error( e.cstr() );
    }
    return n;
}
