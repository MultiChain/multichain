#ifndef JSON_SPIRIT_WRITER_TEMPLATE
#define JSON_SPIRIT_WRITER_TEMPLATE

//          Copyright John W. Wilkinson 2007 - 2009.
// Distributed under the MIT License, see accompanying file LICENSE.txt

// json spirit version 4.03

#include "json_spirit_value.h"

#include <cassert>
#include <sstream>
#include <iomanip>

extern uint32_t JSON_NO_DOUBLE_FORMATTING;                             


namespace json_spirit
{
    
    inline unsigned int utf8_len_and_mask(unsigned char c,unsigned int *mask)
    {
        if(c<0x80){*mask=0x7F;return 1;}
        if(c<0xC0){*mask=0x00;return 0;}
        if(c<0xE0){*mask=0x1F;return 2;}
        if(c<0xF0){*mask=0x0F;return 3;}
        if(c<0xF8){*mask=0x07;return 4;}
        if(c<0xFC){*mask=0x03;return 5;}
        if(c<0xFE){*mask=0x01;return 6;}
        *mask=0x00;return 0;
    }
    
    
    inline char to_hex_char( unsigned int c )
    {
        assert( c <= 0xF );

        const char ch = static_cast< char >( c );

        if( ch < 10 ) return '0' + ch;

        return 'A' - 10 + ch;
    }

    template< class String_type >
    String_type non_printable_to_string( unsigned int c )
    {
        // Silence the warning: typedef ‘Char_type’ locally defined but not used [-Wunused-local-typedefs]
        // typedef typename String_type::value_type Char_type;

        String_type result( 6, '\\' );

        result[1] = 'u';

        result[ 5 ] = to_hex_char( c & 0x000F ); c >>= 4;
        result[ 4 ] = to_hex_char( c & 0x000F ); c >>= 4;
        result[ 3 ] = to_hex_char( c & 0x000F ); c >>= 4;
        result[ 2 ] = to_hex_char( c & 0x000F );

        return result;
    }

    template< typename Char_type, class String_type >
    bool add_esc_char( Char_type c, String_type& s )
    {
        switch( c )
        {
            case '"':  s += to_str< String_type >( "\\\"" ); return true;
            case '\\': s += to_str< String_type >( "\\\\" ); return true;
            case '\b': s += to_str< String_type >( "\\b"  ); return true;
            case '\f': s += to_str< String_type >( "\\f"  ); return true;
            case '\n': s += to_str< String_type >( "\\n"  ); return true;
            case '\r': s += to_str< String_type >( "\\r"  ); return true;
            case '\t': s += to_str< String_type >( "\\t"  ); return true;
        }

        return false;
    }

    template< class String_type >
    String_type codepoint_to_string( unsigned int codepoint )
    {
        unsigned int part1,part2,len;
        if(codepoint >= 0x10000)
        {
            part2=((codepoint-0x10000) & 0x3FF) + 0xDC00;
            part1=(((codepoint-0x10000) >> 10) & 0x3FF) + 0xD800;
            len=12;
        }
        else
        {
            part1=codepoint;
            part2=0;
            len=6;
        }
        
        String_type result( len, '\\' );

        result[1] = 'u';

        result[ 5 ] = to_hex_char( part1 & 0x000F ); part1 >>= 4;
        result[ 4 ] = to_hex_char( part1 & 0x000F ); part1 >>= 4;
        result[ 3 ] = to_hex_char( part1 & 0x000F ); part1 >>= 4;
        result[ 2 ] = to_hex_char( part1 & 0x000F );

        if(part2)
        {
            result[6] = '\\';
            result[7] = 'u';
            
            result[ 11 ] = to_hex_char( part2 & 0x000F ); part2 >>= 4;
            result[ 10 ] = to_hex_char( part2 & 0x000F ); part2 >>= 4;
            result[  9 ] = to_hex_char( part2 & 0x000F ); part2 >>= 4;
            result[  8 ] = to_hex_char( part2 & 0x000F );                        
        }
        
        return result;
    }

    template< class String_type >
    String_type add_esc_chars( const String_type& s )
    {
        typedef typename String_type::const_iterator Iter_type;
        typedef typename String_type::value_type     Char_type;

        String_type result;

        const Iter_type end( s.end() );

        for( Iter_type i = s.begin(); i != end; ++i )
        {
            const Char_type c( *i );

            if( add_esc_char( c, result ) ) continue;

            const wint_t unsigned_c( ( c >= 0 ) ? c : 256 + c );

            if( iswprint( unsigned_c ) )
            {
                result += c;
            }
            else
            {
                unsigned int codepoint=0;
                unsigned int charlen,shift,j,mask;

                charlen=utf8_len_and_mask(unsigned_c,&mask);
                if( end - i >= charlen)
                {
                    shift=6*(charlen-1);
                    codepoint |= ( unsigned_c & mask) << shift;
                    for(j=1;j<charlen;j++)
                    {
                        shift-=6;
                        codepoint |= ( *( ++i ) & 0x3F) << shift;            
                    }
                    result += codepoint_to_string< String_type >( codepoint );
                }
//                result += non_printable_to_string< String_type >( unsigned_c );
            }
        }

        return result;
    }

    // this class generates the JSON text,
    // it keeps track of the indentation level etc.
    //
    template< class Value_type, class Ostream_type >
    class Generator
    {
        typedef typename Value_type::Config_type Config_type;
        typedef typename Config_type::String_type String_type;
        typedef typename Config_type::Object_type Object_type;
        typedef typename Config_type::Array_type Array_type;
        typedef typename String_type::value_type Char_type;
        typedef typename Object_type::value_type Obj_member_type;

    public:

        Generator( const Value_type& value, Ostream_type& os, bool pretty )
        :   os_( os )
        ,   indentation_level_( 0 )
        ,   pretty_( pretty )
        {
            output( value );
        }

    private:

        void output( const Value_type& value )
        {
            switch( value.type() )
            {
                case obj_type:   output( value.get_obj() );   break;
                case array_type: output( value.get_array() ); break;
                case str_type:   output( value.get_str() );   break;
                case bool_type:  output( value.get_bool() );  break;
                case int_type:   output_int( value );         break;

                /// Bitcoin: Added std::fixed and changed precision from 16 to 8
/*                
                case real_type:  os_ << std::showpoint << std::fixed << std::setprecision(8)
                                     << value.get_real();     break;
*/
                case real_type:  output_double( value.get_real() );break;
                case null_type:  os_ << "null";               break;
                default: assert( false );
            }
        }

        void output( const Object_type& obj )
        {
            output_array_or_obj( obj, '{', '}' );
        }

        void output( const Array_type& arr )
        {
            output_array_or_obj( arr, '[', ']' );
        }

        void output( const Obj_member_type& member )
        {
            output( Config_type::get_name( member ) ); space(); 
            os_ << ':'; space(); 
            output( Config_type::get_value( member ) );
        }

        void output_int( const Value_type& value )
        {
            if( value.is_uint64() )
            {
                os_ << value.get_uint64();
            }
            else
            {
               os_ << value.get_int64();
            }
        }

        void output_double( const double& value )
        {
            if(JSON_NO_DOUBLE_FORMATTING)                     
            {
                os_ << std::showpoint << std::fixed << std::setprecision(9) << value;                
                return; 
            }
            double a=fabs(value);
            double e=0.0;
            int z=0;
            double f=0.;
            int j=0;
            if(a > 0)
            {
                e=log10(a);
            }            
            if(e < -4)
            {
                f=a*1.e+9;
                j=(int)f;
                if(j)
                {
                    if( (f-j) < 0.0001)
                    {
                        z=1;
                    }
                }
            }
            int k=(int)e;
            if(e<k)
            {
                k--;
            }
            double v=value/pow(10.,k);
            
            int p=9;
            double t=fabs(v)*pow(10.,p);
            int64_t n=(int64_t)(t+0.5);
            int64_t m=(int64_t)(t/10+0.5);
            while( (p>0) && (n == m*10))
            {
                p--;
                t/=10.;
                n=m;
                m=(int64_t)(t/10.+0.5);
            }
            
            if(p-k > 9)
            {
                z=0;
            }
            
            if( ((e < -4.) || (e > 12.)) && (z == 0))
            {
                if(p > 0)
                {
                    os_ << std::showpoint << std::fixed << std::setprecision(p) << v;
                }
                else
                {
                    os_ << (int)v;
                }
                os_ << "e" << ((e>=0) ? "+" : "") << k;
            }
            else
            {
                p-=k;
                if(p > 0)
                {
                    os_ << std::showpoint << std::fixed << std::setprecision(p) << value;
                }
                else
                {
                    os_ << (int64_t)value;                    
                }
            }
        }
        
        void output( const String_type& s )
        {
            os_ << '"' << add_esc_chars( s ) << '"';
        }

        void output( bool b )
        {
            os_ << to_str< String_type >( b ? "true" : "false" );
        }

        template< class T >
        void output_array_or_obj( const T& t, Char_type start_char, Char_type end_char )
        {
            os_ << start_char; new_line();

            ++indentation_level_;
            
            for( typename T::const_iterator i = t.begin(); i != t.end(); ++i )
            {
                indent(); output( *i );

                typename T::const_iterator next = i;

                if( ++next != t.end())
                {
                    os_ << ',';
                }

                new_line();
            }

            --indentation_level_;

            indent(); os_ << end_char;
        }
        
        void indent()
        {
            if( !pretty_ ) return;

            for( int i = 0; i < indentation_level_; ++i )
            { 
                os_ << "    ";
            }
        }

        void space()
        {
            if( pretty_ ) os_ << ' ';
        }

        void new_line()
        {
            if( pretty_ ) os_ << '\n';
        }

        Generator& operator=( const Generator& ); // to prevent "assignment operator could not be generated" warning

        Ostream_type& os_;
        int indentation_level_;
        bool pretty_;
    };

    template< class Value_type, class Ostream_type >
    void write_stream( const Value_type& value, Ostream_type& os, bool pretty )
    {
        Generator< Value_type, Ostream_type >( value, os, pretty );
    }

    template< class Value_type >
    typename Value_type::String_type write_string( const Value_type& value, bool pretty )
    {
        typedef typename Value_type::String_type::value_type Char_type;

        std::basic_ostringstream< Char_type > os;

        write_stream( value, os, pretty );

        return os.str();
    }
}

#endif
