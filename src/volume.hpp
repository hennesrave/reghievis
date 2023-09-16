#pragma once
#include "utility.hpp"
#include "math.hpp"

#include <qstring.h>
#include <qopenglcontext.h>
#include <qopenglfunctions_4_5_core.h>

#include <vector>

// Class to manage a volume
template<typename T> class Volume
{
public:
	Volume() noexcept = default;

	// Create a volume from specified dimensions, values, name, or read it from a file
	Volume( vec3i dimensions, QString name = "" ) : _name( std::move( name ) ), _dimensions( dimensions ), _values( dimensions.product() )
	{}
	Volume( vec3i dimensions, std::vector<T> values, QString name = "" ) : _name( std::move( name ) ), _dimensions( dimensions ), _values( std::move( values ) )
	{
		if( dimensions.product() != _values.size() )
			throw std::runtime_error( "Volume::Volume( vec3i, std::vector<T> ) -> Dimensions dont match size of vector." );
	}
	Volume( std::ifstream& stream )
	{
		std::string name;
		util::read_binary( stream, name );
		_name = QString::fromStdString( name );

		util::read_binary( stream, _dimensions );
		util::read_binary_vector( stream, _values );
		util::read_binary( stream, _domain );
		_domainValid = true;
	}

	// Copy and move constructors
	Volume( const Volume& other ) : _name( other._name ), _dimensions( other._dimensions ), _values( other._values ), _domain( other._domain ), _domainValid( other._domainValid ), _texture( 0 ), _textureValid( false )
	{}
	Volume( Volume&& other ) : _name( std::move( other._name ) ), _dimensions( other._dimensions ), _values( std::move( other._values ) ), _domain( other._domain ), _domainValid( other._domainValid ), _texture( other._texture ), _textureValid( other._textureValid )
	{
		other._texture = 0;
		other._textureValid = false;
	}

	// Copy and move assignment operators
	Volume& operator=( const Volume& other )
	{
		_name = other._name;
		_dimensions = other._dimensions;
		_values = other._values;
		_domain = other._domain;
		_domainValid = other._domainValid;
		_texture = 0;
		_textureValid = false;
		return *this;
	}
	Volume& operator=( Volume&& other )
	{
		_name = std::move( other._name );
		_dimensions = other._dimensions;
		_values = std::move( other._values );
		_domain = other._domain;
		_domainValid = other._domainValid;
		_texture = other._texture;
		_textureValid = other._textureValid;

		other._texture = 0;
		other._textureValid = false;
		return *this;
	}

	// Destructor :)
	virtual ~Volume()
	{
		if( _texture )
		{
			const auto context = QOpenGLContext::globalShareContext();
			const auto functions = context->versionFunctions<QOpenGLFunctions_4_5_Core>();
			functions->glDeleteTextures( 1, &_texture );

			_texture = 0;
			_textureValid = false;
		}
	}

	// Save volume to file
	void save( std::ofstream& stream ) const
	{
		util::write_binary( stream, _name.toStdString() );
		util::write_binary( stream, _dimensions );
		util::write_binary_vector( stream, _values );

		if constexpr( std::is_arithmetic<T>::value ) auto domain = this->domain();
		util::write_binary( stream, _domain );
	}

	// Getter and setter for the name
	void setName( QString name )
	{
		_name = std::move( name );
	}
	const QString& name() const noexcept
	{
		return _name;
	}

	// Getter and setter for the volume values
	void setValues( std::vector<T> values )
	{
		if( values.size() != _values.size() ) throw std::runtime_error( "Volume::setValues -> Wrong number of values were given." );
		_values = std::move( values );
		this->invalidate();
	}
	const std::vector<T>& values() const noexcept
	{
		return _values;
	}

	// Getters for basic statistics
	vec3i dimensions() const noexcept
	{
		return _dimensions;
	}
	int32_t voxelCount() const noexcept
	{
		return _values.size();
	}

	// Data pointer getters
	const T* data() const noexcept
	{
		return _values.data();
	}
	T* data() noexcept
	{
		return _values.data();
	}

	const T* begin() const noexcept
	{
		return _values.data();
	}
	T* begin() noexcept
	{
		return _values.data();
	}

	const T* end() const noexcept
	{
		return _values.data() + _values.size();
	}
	T* end() noexcept
	{
		return _values.data() + _values.size();
	}

	// Convert a 3D voxel position to an index into the underlying data vector
	int32_t voxelToIndex( vec3i coords ) const noexcept
	{
		return _dimensions.z * ( coords.y + _dimensions.y * coords.x ) + coords.z;
	}

	// Element access
	const T& at( int32_t index ) const
	{
		return _values[index];
	}
	T& at( int32_t index )
	{
		return _values[index];
	}

	const T& at( vec3i coords ) const
	{
		return _values[this->voxelToIndex( coords )];
	}
	T& at( vec3i coords )
	{
		return  _values[this->voxelToIndex( coords )];
	}

	// Getter for the value domain (for example, some volume should always have the domain [0,1])
	vec2<T> domain() const noexcept
	{
		static_assert( std::is_arithmetic<T>::value, "Type must be arithmetic to compute domain." );

		// The domain is cached, so compute it if its not available (lazy evaluation)
		if( !_domainValid )
		{
			const auto [min, max] = std::minmax_element( _values.begin(), _values.end() );
			_domain = vec2<T>( *min, *max );
			_domainValid = true;
		}

		return _domain;
	}

	// Expand the domain if possible
	void expandDomain( vec2<T> expansion )
	{
		const auto domain = this->domain();
		expansion.x = std::min( expansion.x, domain.x );
		expansion.y = std::max( expansion.y, domain.y );
		_domain = expansion;
	}

	// Return the underlying OpenGL texture or invalidate it
	GLuint texture() const
	{
		static_assert( std::is_same<T, float>::value, "Type must be floating point to use texture." );

		// The texture is cached, so compute create it if its not available (lazy evaluation)
		if( !_textureValid )
		{
			const auto context = QOpenGLContext::globalShareContext();
			const auto functions = context->versionFunctions<QOpenGLFunctions_4_5_Core>();

			if( !_texture )
			{
				functions->glGenTextures( 1, &_texture );
				functions->glBindTexture( GL_TEXTURE_3D, _texture );
				functions->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0 );
				functions->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
				functions->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
				functions->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
				functions->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
				functions->glTexParameteri( GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
			}

			functions->glTexImage3D( GL_TEXTURE_3D, 0, GL_R32F, _dimensions.x, _dimensions.y, _dimensions.z, 0, GL_RED, GL_FLOAT, _values.data() );
			_textureValid = true;
		}
		return _texture;
	}
	void invalidate()
	{
		_domainValid = false;
		_textureValid = false;
	}

	// Compare two volumes
	bool operator==( const Volume& other ) const
	{
		if( _name != other._name ) return false;
		if( _dimensions != other._dimensions ) return false;
		for( size_t i = 0; i < _values.size(); ++i ) if( _values[i] != other._values[i] ) return false;
		if( _domain != other._domain ) return false;
		return true;
	}
	bool operator!=( const Volume& other ) const
	{
		return !this->operator==( other );
	}

	// Simply map one volume to another using a conversion function
	template<typename U> Volume<U> map( const std::function<U( T )>& conversion ) const
	{
		auto result = Volume<U>( _dimensions, _name );
		util::compute_multi_threaded( 0, _values.size(), [&] ( int32_t begin, int32_t end )
		{
			for( int32_t i = begin; i < end; ++i )
				result.at( i ) = conversion( this->at( i ) );
		} );
		return result;
	}
	template<typename U> Volume<U> cast() const
	{
		auto result = Volume<U>( _dimensions, _name );
		util::compute_multi_threaded( 0, _values.size(), [&] ( int32_t begin, int32_t end )
		{
			for( int32_t i = begin; i < end; ++i )
				result.at( i ) = static_cast<U>( this->at( i ) );
		} );
		return result;
	}

private:
	QString _name;
	vec3i _dimensions;
	std::vector<T> _values;

	mutable vec2<T> _domain;
	mutable bool _domainValid = false;

	mutable GLuint _texture = 0;
	mutable bool _textureValid = false;
};