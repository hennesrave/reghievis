#pragma once
#include <fstream>
#include <thread>
#include <vector>

#include <qlayout.h>
#include "math.hpp"

namespace util
{
	// Helper functions for writing objects to a binary file
	template<typename T> void write_binary( std::ofstream& stream, const T& value )
	{
		stream.write( reinterpret_cast<const char*>( &value ), sizeof( T ) );
	}
	template<typename T> void read_binary( std::ifstream& stream, T& value )
	{
		stream.read( reinterpret_cast<char*>( &value ), sizeof( value ) );
	}
	template<typename T> T read_binary( std::ifstream& stream )
	{
		T value;
		stream.read( reinterpret_cast<char*>( &value ), sizeof( value ) );
		return value;
	}

	template<> inline void write_binary<std::string>( std::ofstream& stream, const std::string& string )
	{
		const auto size = string.size();
		stream.write( reinterpret_cast<const char*>( &size ), sizeof( size ) );
		stream.write( reinterpret_cast<const char*>( string.data() ), string.size() );
	}
	template<> inline void read_binary<std::string>( std::ifstream& stream, std::string& string )
	{
		size_t size;
		stream.read( reinterpret_cast<char*>( &size ), sizeof( size ) );

		string = std::string( size, '\0' );
		stream.read( reinterpret_cast<char*>( string.data() ), size );
	}

	template<typename T> void write_binary_vector( std::ofstream& stream, const std::vector<T>& values )
	{
		const auto size = values.size();
		stream.write( reinterpret_cast<const char*>( &size ), sizeof( size ) );
		stream.write( reinterpret_cast<const char*>( values.data() ), values.size() * sizeof( T ) );
	}
	template<typename T> void read_binary_vector( std::ifstream& stream, std::vector<T>& values )
	{
		size_t size;
		stream.read( reinterpret_cast<char*>( &size ), sizeof( size ) );

		values = std::vector<T>( size );
		stream.read( reinterpret_cast<char*>( values.data() ), size * sizeof( T ) );
	}

	// Helper function to combine two hashes (https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x)
	template <class T> size_t hash_combine( size_t seed, const T& v )
	{
		return seed ^ ( std::hash<T>()( v ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 ) );
	}

	// Convert RGB to LAB (http://www.easyrgb.com/index.php?X=MATH&H=01#text1)
	inline vec3f rgb2lab( vec3f rgb )
	{
		if( rgb.x > 0.04045 ) rgb.x = pow( ( ( rgb.x + 0.055 ) / 1.055 ), 2.4 );
		else                   rgb.x = rgb.x / 12.92;
		if( rgb.y > 0.04045 ) rgb.y = pow( ( ( rgb.y + 0.055 ) / 1.055 ), 2.4 );
		else                   rgb.y = rgb.y / 12.92;
		if( rgb.z > 0.04045 ) rgb.z = pow( ( ( rgb.z + 0.055 ) / 1.055 ), 2.4 );
		else                   rgb.z = rgb.z / 12.92;

		rgb.x = rgb.x * 100.;
		rgb.y = rgb.y * 100.;
		rgb.z = rgb.z * 100.;

		//Observer. = 2°, Illuminant = D65
		float X = rgb.x * 0.4124 + rgb.y * 0.3576 + rgb.z * 0.1805;
		float Y = rgb.x * 0.2126 + rgb.y * 0.7152 + rgb.z * 0.0722;
		float Z = rgb.x * 0.0193 + rgb.y * 0.1192 + rgb.z * 0.9505;


		float var_X = X / 95.047;         //ref_X =  95.047   Observer= 2°, Illuminant= D65
		float var_Y = Y / 100.000;          //ref_Y = 100.000
		float var_Z = Z / 108.883;          //ref_Z = 108.883

		if( var_X > 0.008856 ) var_X = pow( var_X, ( 1. / 3. ) );
		else                    var_X = ( 7.787 * var_X ) + ( 16. / 116. );
		if( var_Y > 0.008856 ) var_Y = pow( var_Y, ( 1. / 3. ) );
		else                    var_Y = ( 7.787 * var_Y ) + ( 16. / 116. );
		if( var_Z > 0.008856 ) var_Z = pow( var_Z, ( 1. / 3. ) );
		else                    var_Z = ( 7.787 * var_Z ) + ( 16. / 116. );

		float l_s = ( 116. * var_Y ) - 16.;
		float a_s = 500. * ( var_X - var_Y );
		float b_s = 200. * ( var_Y - var_Z );

		return vec3f( l_s, a_s, b_s );
	}

	// Conver LAB to RGB (http://www.easyrgb.com/index.php?X=MATH&H=01#text1)
	inline vec3f lab2rgb( vec3f lab )
	{
		float var_Y = ( lab.x + 16. ) / 116.;
		float var_X = lab.y / 500. + var_Y;
		float var_Z = var_Y - lab.z / 200.;

		if( pow( var_Y, 3 ) > 0.008856 ) var_Y = pow( var_Y, 3 );
		else                      var_Y = ( var_Y - 16. / 116. ) / 7.787;
		if( pow( var_X, 3 ) > 0.008856 ) var_X = pow( var_X, 3 );
		else                      var_X = ( var_X - 16. / 116. ) / 7.787;
		if( pow( var_Z, 3 ) > 0.008856 ) var_Z = pow( var_Z, 3 );
		else                      var_Z = ( var_Z - 16. / 116. ) / 7.787;

		float X = 95.047 * var_X;    //ref_X =  95.047     Observer= 2°, Illuminant= D65
		float Y = 100.000 * var_Y;   //ref_Y = 100.000
		float Z = 108.883 * var_Z;    //ref_Z = 108.883


		var_X = X / 100.;       //X from 0 to  95.047      (Observer = 2°, Illuminant = D65)
		var_Y = Y / 100.;       //Y from 0 to 100.000
		var_Z = Z / 100.;      //Z from 0 to 108.883

		float var_R = var_X * 3.2406 + var_Y * -1.5372 + var_Z * -0.4986;
		float var_G = var_X * -0.9689 + var_Y * 1.8758 + var_Z * 0.0415;
		float var_B = var_X * 0.0557 + var_Y * -0.2040 + var_Z * 1.0570;

		if( var_R > 0.0031308 ) var_R = 1.055 * pow( var_R, ( 1 / 2.4 ) ) - 0.055;
		else                     var_R = 12.92 * var_R;
		if( var_G > 0.0031308 ) var_G = 1.055 * pow( var_G, ( 1 / 2.4 ) ) - 0.055;
		else                     var_G = 12.92 * var_G;
		if( var_B > 0.0031308 ) var_B = 1.055 * pow( var_B, ( 1 / 2.4 ) ) - 0.055;
		else                     var_B = 12.92 * var_B;

		return vec3f( var_R, var_G, var_B );
	}

	// Helper function to easily setup parallel computation
	inline void compute_multi_threaded( const std::function<void( int32_t, int32_t )>& function )
	{
		auto threads = std::vector<std::thread>( std::thread::hardware_concurrency() );
		for( int32_t i = 0; i < threads.size(); ++i ) threads[i] = std::thread( function, i, static_cast<int32_t>( threads.size() ) );
		for( auto& thread : threads ) thread.join();
	}
	inline void compute_multi_threaded( int32_t begin, int32_t end, const std::function<void( int32_t, int32_t )>& function )
	{
		auto threads = std::vector<std::thread>( std::thread::hardware_concurrency() );
		int32_t step = ( end - begin ) / threads.size();
		for( int32_t i = 0; i < threads.size(); ++i )
		{
			threads[i] = std::thread( function, begin, ( i == threads.size() - 1 ) ? end : begin + step );
			begin += step;
		}
		for( auto& thread : threads ) thread.join();
	}

	// Helper function to easily create box layouts in a single line
	inline QBoxLayout* createBoxLayout( QBoxLayout::Direction direction, int left, int top, int right, int bottom, int spacing = 5, const std::vector<QWidget*>& widgets = {}, const std::vector<int>& stretch = {} )
	{
		auto layout = new QBoxLayout( direction );
		layout->setContentsMargins( left, top, right, bottom );
		layout->setSpacing( spacing );
		for( int32_t i = 0; i < widgets.size(); ++i ) layout->addWidget( widgets[i], stretch[i] );
		return layout;
	}
	inline QBoxLayout* createBoxLayout( QBoxLayout::Direction direction, int spacing, const std::vector<QWidget*>& widgets, const std::vector<int>& stretch )
	{
		return util::createBoxLayout( direction, 0, 0, 0, 0, spacing, widgets, stretch );
	}
	inline QBoxLayout* createBoxLayout( QBoxLayout::Direction direction, int spacing = 5, const std::vector<QWidget*>& widgets = {} )
	{
		return util::createBoxLayout( direction, 0, 0, 0, 0, spacing, widgets, std::vector<int>( widgets.size(), 0 ) );
	}

	// Helper class to measure time
	class timer
	{
	public:
		double get() const
		{
			const auto end = std::chrono::high_resolution_clock::now();
			return std::chrono::duration_cast<std::chrono::microseconds>( end - _begin ).count() / 1000.0;
		}
		double reset()
		{
			const auto time = this->get();
			_begin = std::chrono::high_resolution_clock::now();
			return time;
		}

	private:
		decltype( std::chrono::high_resolution_clock::now() ) _begin = std::chrono::high_resolution_clock::now();
	};
}