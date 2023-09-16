#pragma once
#include <qobject.h>
#include <qopenglcontext.h>
#include <qopenglfunctions.h>
#include <qopenglfunctions_4_5_core.h>

#include <unordered_set>

#include "ensemble.hpp"

// Class for managing a region (a subset of voxels within a volume)
class Region : public QObject
{
	Q_OBJECT
public:
	// Create the default region with the specified name
	Region( QString name ) : _name( std::move( name ) )
	{}

	// Copy a region, giving it another name
	Region( QString name, const Region& other ) : _name( std::move( name ) ), _intervals( other._intervals ), _enabledAxes( other._enabledAxes ), _constantMask( other._constantMask ? new Volume<float>( *other._constantMask ) : nullptr )
	{
		if( other._selectionBuffer )
		{
			const auto context = QOpenGLContext::globalShareContext();
			const auto functions = context->versionFunctions<QOpenGLFunctions_4_5_Core>();
			functions->glGenBuffers( 1, &_selectionBuffer );

			GLint size = 0;
			functions->glBindBuffer( GL_COPY_READ_BUFFER, other._selectionBuffer );
			functions->glGetBufferParameteriv( GL_COPY_READ_BUFFER, GL_BUFFER_SIZE, &size );

			functions->glBindBuffer( GL_COPY_WRITE_BUFFER, _selectionBuffer );
			functions->glBufferData( GL_COPY_WRITE_BUFFER, size, nullptr, GL_DYNAMIC_DRAW );
			functions->glCopyBufferSubData( GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, size );
		}
	}

	// Return the specified intervals for various derived volumes (for parallel coordinates axes)
	std::unordered_map<Ensemble::VolumeID, std::vector<vec2d>>& intervals()
	{
		return _intervals;
	}
	std::vector<vec2d>& intervals( Ensemble::VolumeID type )
	{
		return _intervals[type];
	}

	// Return the axes that the user enabled for this region
	std::unordered_set<Ensemble::VolumeID>& enabledAxes() noexcept
	{
		return _enabledAxes;
	}

	// Getter and setter for the region's name
	void setName( QString name )
	{
		_name = std::move( name );
		emit nameChanged( _name );
	}
	const QString& name() const noexcept
	{
		return _name;
	}

	// Setter and getter for the constant mask (as opposed to a mask based on brushing)
	void setConstantMask( std::shared_ptr<Volume<float>> mask )
	{
		if( std::count( mask->begin(), mask->end(), 0.0f ) == mask->voxelCount() ) _constantMask.reset();
		else _constantMask = mask;

		const auto context = QOpenGLContext::globalShareContext();
		const auto functions = context->functions();
		if( !_selectionBuffer ) functions->glGenBuffers( 1, &_selectionBuffer );
		functions->glBindBuffer( GL_SHADER_STORAGE_BUFFER, _selectionBuffer );
		functions->glBufferData( GL_SHADER_STORAGE_BUFFER, mask->voxelCount() * sizeof( float ), mask->data(), GL_STATIC_DRAW );

		for( auto& [key, intervals] : _intervals ) intervals.clear();
		emit selectionChanged();
	}
	std::shared_ptr<Volume<float>> constantMask() const noexcept
	{
		return _constantMask;
	}
	void clearConstantMask()
	{
		_constantMask.reset();
	}

	// Return the OpenGL buffer that will contain the binary selection mask
	GLuint selectionBuffer() const
	{
		if( _selectionBuffer == 0 )
		{
			const auto context = QOpenGLContext::globalShareContext();
			const auto functions = context->functions();
			functions->glGenBuffers( 1, &_selectionBuffer );
		}
		return _selectionBuffer;
	}

	// Return the binary volume defined by the region using the specified ensemble
	std::shared_ptr<Volume<float>> createMask( const Ensemble& ensemble ) const
	{
		// If the mask is constant (independent of an ensemble), return it
		if( _constantMask ) return _constantMask;

		// Prepare mask
		auto mask = std::shared_ptr<Volume<float>>( new Volume<float>( ensemble.dimensions() ) );
		std::fill( mask->begin(), mask->end(), 1.0f );

		// Compute the mask using the selected intervals (brushing)
		for( const auto& [type, intervals_sb] : _intervals ) if( _enabledAxes.count( type ) && intervals_sb.size() )
		{
			const auto& intervals = intervals_sb;
			const auto& volume = ensemble.volume( type );
			util::compute_multi_threaded( 0, mask->voxelCount(), [&] ( int32_t begin, int32_t end )
			{
				for( int32_t i = begin; i < end; ++i ) if( mask->at( i ) != 0.0f )
				{
					const auto value = volume.at( i );

					auto selected = false;
					for( const auto [lower, upper] : intervals )
					{
						if( value >= lower && value <= upper )
						{
							selected = true;
							break;
						}
					}
					if( !selected ) mask->at( i ) = 0.0f;
				}
			} );
		}

		// Expand domain to [0, 1] in case all values are the same (domain where min == max can cause issues)
		mask->expandDomain( vec2f( 0.0f, 1.0f ) );
		return mask;
	}

signals:
	void selectionChanged();
	void nameChanged( const QString& );

private:
	QString _name;
	std::unordered_map<Ensemble::VolumeID, std::vector<vec2d>> _intervals;
	std::unordered_set<Ensemble::VolumeID> _enabledAxes;
	std::shared_ptr<Volume<float>> _constantMask;

	mutable GLuint _selectionBuffer = 0;
};