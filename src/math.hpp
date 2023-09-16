#pragma once
#include <cmath>
#include <ostream>

// Geometry classes for 2D, 3D and 4D vectors

template<typename T> class vec2
{
public:
	T x, y;

	vec2( T x = 0, T y = 0 ) noexcept : x( x ), y( y ) {}

	template<typename U> operator vec2<U>() const noexcept
	{
		return vec2<U>( static_cast<U>( x ), static_cast<U>( y ) );
	}

	T dot( vec2 other ) const noexcept
	{
		return x * other.x + y * other.y;
	}
	T lengthSq() const noexcept
	{
		return x * x + y * y;
	}

	T sum() const noexcept
	{
		return x + y;
	}
	T product() const noexcept
	{
		return x * y;
	}

	float length() const
	{
		return std::sqrtf( static_cast<float>( this->lengthSq() ) );
	}
	vec2<float> normalized() const
	{
		return static_cast<vec2<float>>( *this ) / this->length();
	}

	T operator[]( int32_t index ) const
	{
		return reinterpret_cast<const T*>( this )[index];
	}
	T& operator[]( int32_t index )
	{
		return reinterpret_cast<T*>( this )[index];
	}

	bool operator==( vec2 other ) const noexcept
	{
		return x == other.x && y == other.y;
	}
	bool operator!=( vec2 other ) const noexcept
	{
		return x != other.x || y != other.y;
	}

	vec2 operator+( vec2 other ) const noexcept
	{
		return vec2( x + other.x, y + other.y );
	}
	vec2 operator-( vec2 other ) const noexcept
	{
		return vec2( x - other.x, y - other.y );
	}
	vec2 operator*( vec2 other ) const noexcept
	{
		return vec2( x * other.x, y * other.y );
	}
	vec2 operator/( vec2 other ) const noexcept
	{
		return vec2( x / other.x, y / other.y );
	}

	vec2& operator+=( vec2 other ) noexcept
	{
		return *this = *this + other;
	}
	vec2& operator-=( vec2 other ) noexcept
	{
		return *this = *this - other;
	}
	vec2& operator*=( vec2 other ) noexcept
	{
		return *this = *this * other;
	}
	vec2& operator/=( vec2 other ) noexcept
	{
		return *this = *this / other;
	}

	vec2 operator+( T value ) const noexcept
	{
		return vec2( x + value, y + value );
	}
	vec2 operator-( T value ) const noexcept
	{
		return vec2( x - value, y - value );
	}
	vec2 operator*( T value ) const noexcept
	{
		return vec2( x * value, y * value );
	}
	vec2 operator/( T value ) const noexcept
	{
		return vec2( x / value, y / value );
	}

	vec2& operator+=( T value ) noexcept
	{
		return *this += value;
	}
	vec2& operator-=( T value ) noexcept
	{
		return *this -= value;
	}
	vec2& operator*=( T value ) noexcept
	{
		return *this *= value;
	}
	vec2& operator/=( T value ) noexcept
	{
		return *this /= value;
	}

	friend vec2 operator+( T value, vec2 vec )
	{
		return vec2( value + vec.x, value + vec.y );
	}
	friend vec2 operator-( T value, vec2 vec )
	{
		return vec2( value - vec.x, value - vec.y );
	}
	friend vec2 operator*( T value, vec2 vec )
	{
		return vec2( value * vec.x, value * vec.y );
	}
	friend vec2 operator/( T value, vec2 vec )
	{
		return vec2( value / vec.x, value / vec.y );
	}

	friend std::ostream& operator<<( std::ostream& stream, vec2 vec )
	{
		return stream << '(' << vec.x << ", " << vec.y << ')';
	}
};

template<typename T> class vec3
{
public:
	T x, y, z;

	vec3( T x = 0, T y = 0, T z = 0 ) noexcept : x( x ), y( y ), z( z ) {}
	vec3( vec2<T> xy, T z = 0 ) noexcept : vec3( xy.x, xy.y, z ) {}
	vec3( T x, vec2<T> yz ) noexcept : vec3( x, yz.x, yz.y ) {}

	template<typename U> operator vec3<U>() const noexcept
	{
		return vec3<U>( static_cast<U>( x ), static_cast<U>( y ), static_cast<U>( z ) );
	}

	vec2<T> xy() const noexcept
	{
		return vec2( x, y );
	}
	vec2<T> yx() const noexcept
	{
		return vec2( y, x );
	}

	vec2<T> yz() const noexcept
	{
		return vec2( y, z );
	}
	vec2<T> zy() const noexcept
	{
		return vec2( z, y );
	}

	vec2<T> xz() const noexcept
	{
		return vec2( x, z );
	}
	vec2<T> zx() const noexcept
	{
		return vec2( z, x );
	}

	T dot( vec3 other ) const noexcept
	{
		return x * other.x + y * other.y + z * other.z;
	}
	T lengthSq() const noexcept
	{
		return x * x + y * y + z * z;
	}

	T sum() const noexcept
	{
		return x + y + z;
	}
	T product() const noexcept
	{
		return x * y * z;
	}

	float length() const
	{
		return std::sqrtf( static_cast<float>( this->lengthSq() ) );
	}
	vec3<float> normalized() const
	{
		return static_cast<vec3<float>>( *this ) / this->length();
	}

	vec3 cross( vec3 other ) const noexcept
	{
		return vec3( y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x );
	}

	T operator[]( int32_t index ) const
	{
		return reinterpret_cast<const T*>( this )[index];
	}
	T& operator[]( int32_t index )
	{
		return reinterpret_cast<T*>( this )[index];
	}

	bool operator==( vec3 other ) const noexcept
	{
		return x == other.x && y == other.y && z == other.z;
	}
	bool operator!=( vec3 other ) const noexcept
	{
		return x != other.x || y != other.y || z != other.z;
	}

	vec3 operator+( vec3 other ) const noexcept
	{
		return vec3( x + other.x, y + other.y, z + other.z );
	}
	vec3 operator-( vec3 other ) const noexcept
	{
		return vec3( x - other.x, y - other.y, z - other.z );
	}
	vec3 operator*( vec3 other ) const noexcept
	{
		return vec3( x * other.x, y * other.y, z * other.z );
	}
	vec3 operator/( vec3 other ) const noexcept
	{
		return vec3( x / other.x, y / other.y, z / other.z );
	}

	vec3& operator+=( vec3 other ) noexcept
	{
		return *this = *this + other;
	}
	vec3& operator-=( vec3 other ) noexcept
	{
		return *this = *this - other;
	}
	vec3& operator*=( vec3 other ) noexcept
	{
		return *this = *this * other;
	}
	vec3& operator/=( vec3 other ) noexcept
	{
		return *this = *this / other;
	}

	vec3 operator+( T value ) const noexcept
	{
		return vec3( x + value, y + value, z + value );
	}
	vec3 operator-( T value ) const noexcept
	{
		return vec3( x - value, y - value, z - value );
	}
	vec3 operator*( T value ) const noexcept
	{
		return vec3( x * value, y * value, z * value );
	}
	vec3 operator/( T value ) const noexcept
	{
		return vec3( x / value, y / value, z / value );
	}

	vec3& operator+=( T value ) noexcept
	{
		return *this += value;
	}
	vec3& operator-=( T value ) noexcept
	{
		return *this -= value;
	}
	vec3& operator*=( T value ) noexcept
	{
		return *this *= value;
	}
	vec3& operator/=( T value ) noexcept
	{
		return *this /= value;
	}

	friend vec3 operator+( T value, vec3 vec )
	{
		return vec3( value + vec.x, value + vec.y, value + vec.z );
	}
	friend vec3 operator-( T value, vec3 vec )
	{
		return vec3( value - vec.x, value - vec.y, value - vec.z );
	}
	friend vec3 operator*( T value, vec3 vec )
	{
		return vec3( value * vec.x, value * vec.y, value * vec.z );
	}
	friend vec3 operator/( T value, vec3 vec )
	{
		return vec3( value / vec.x, value / vec.y, value / vec.z );
	}

	friend std::ostream& operator<<( std::ostream& stream, vec3 vec )
	{
		return stream << '(' << vec.x << ", " << vec.y << ", " << vec.z << ')';
	}
};

template<typename T> class vec4
{
public:
	T x, y, z, w;

	vec4( T x = 0, T y = 0, T z = 0, T w = 0 ) noexcept : x( x ), y( y ), z( z ), w( w ) {}

	template<typename U> operator vec4<U>() const noexcept
	{
		return vec4<U>( static_cast<U>( x ), static_cast<U>( y ), static_cast<U>( z ), static_cast<U>( w ) );
	}

	T dot( vec4 other ) const noexcept
	{
		return x * other.x + y * other.y + z * other.z + w * other.w;
	}
	T lengthSq() const noexcept
	{
		return x * x + y * y + z * z + w * w;
	}

	T sum() const noexcept
	{
		return x + y + z + w;
	}
	T product() const noexcept
	{
		return x * y * z * w;
	}

	float length() const
	{
		return std::sqrtf( static_cast<float>( this->lengthSq() ) );
	}
	vec4<float> normalized() const
	{
		return static_cast<vec4<float>>( *this ) / this->length();
	}

	T operator[]( int32_t index ) const
	{
		return reinterpret_cast<const T*>( this )[index];
	}
	T& operator[]( int32_t index )
	{
		return reinterpret_cast<T*>( this )[index];
	}

	bool operator==( vec4 other ) const noexcept
	{
		return x == other.x && y == other.y && z == other.z && w == other.w;
	}
	bool operator!=( vec4 other ) const noexcept
	{
		return x != other.x || y != other.y || z != other.z || w != other.w;
	}

	vec4 operator+( vec4 other ) const noexcept
	{
		return vec4( x + other.x, y + other.y, z + other.z, w + other.w );
	}
	vec4 operator-( vec4 other ) const noexcept
	{
		return vec4( x - other.x, y - other.y, z - other.z, w - other.w );
	}
	vec4 operator*( vec4 other ) const noexcept
	{
		return vec4( x * other.x, y * other.y, z * other.z, w * other.w );
	}
	vec4 operator/( vec4 other ) const noexcept
	{
		return vec4( x / other.x, y / other.y, z / other.z, w / other.w );
	}

	vec4& operator+=( vec4 other ) noexcept
	{
		return *this = *this + other;
	}
	vec4& operator-=( vec4 other ) noexcept
	{
		return *this = *this - other;
	}
	vec4& operator*=( vec4 other ) noexcept
	{
		return *this = *this * other;
	}
	vec4& operator/=( vec4 other ) noexcept
	{
		return *this = *this / other;
	}

	vec4 operator+( T value ) const noexcept
	{
		return vec4( x + value, y + value, z + value, w + value );
	}
	vec4 operator-( T value ) const noexcept
	{
		return vec4( x - value, y - value, z - value, w - value );
	}
	vec4 operator*( T value ) const noexcept
	{
		return vec4( x * value, y * value, z * value, w * value );
	}
	vec4 operator/( T value ) const noexcept
	{
		return vec4( x / value, y / value, z / value, w / value );
	}

	vec4& operator+=( T value ) noexcept
	{
		return *this += value;
	}
	vec4& operator-=( T value ) noexcept
	{
		return *this -= value;
	}
	vec4& operator*=( T value ) noexcept
	{
		return *this *= value;
	}
	vec4& operator/=( T value ) noexcept
	{
		return *this /= value;
	}

	friend vec4 operator+( T value, vec4 vec )
	{
		return vec4( value + vec.x, value + vec.y, value + vec.z, value + vec.w );
	}
	friend vec4 operator-( T value, vec4 vec )
	{
		return vec4( value - vec.x, value - vec.y, value - vec.z, value - vec.w );
	}
	friend vec4 operator*( T value, vec4 vec )
	{
		return vec4( value * vec.x, value * vec.y, value * vec.z, value * vec.w );
	}
	friend vec4 operator/( T value, vec4 vec )
	{
		return vec4( value / vec.x, value / vec.y, value / vec.z, value / vec.w );
	}

	friend std::ostream& operator<<( std::ostream& stream, vec4 vec )
	{
		return stream << '(' << vec.x << ", " << vec.y << ", " << vec.z << ", " << vec.w << ')';
	}
};

using vec2f = vec2<float>;
using vec2d = vec2<double>;
using vec2i = vec2<int32_t>;

using vec3f = vec3<float>;
using vec3d = vec3<double>;
using vec3i = vec3<int32_t>;

using vec4f = vec4<float>;
using vec4d = vec4<double>;
using vec4i = vec4<int32_t>;