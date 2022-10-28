#pragma once
#include <qevent.h>
#include <qlabel.h>
#include <qmath.h>
#include <qopenglfunctions_4_5_core.h>
#include <qopenglshaderprogram.h>
#include <qopenglwidget.h>
#include <qpainter.h>
#include <qwidget.h>

#include <iostream>
#include <stack>
#include <unordered_set>

#include <renderdoc_app.h>

#include "color_map.hpp"
#include "dendrogram.hpp"
#include "ensemble.hpp"
#include "parallel_coordinates.hpp"

// Enum for the filtering mode of input volume in the ray casting algorithm
enum class Filtering { eNearest, eLinear };

// Enum for the composition mode in the ray casting algorithm
enum class Compositing { eAlphaBlending, eFirstHit, eMaximumIntensity, eFirstLocalMaximum };

// Class to manage settings shared between all volume renderers
class VolumeRendererSettings : public QObject
{
	Q_OBJECT
public:
	// Enum class for the visualization type of a volume renderer
	enum class Visualization { e3D, eSlice };

	// Struct for the camera
	struct Camera
	{
		vec3f position, lookAt, forward, right, up;
	};

	VolumeRendererSettings()
	{
		this->setCamera( vec3f( 0.5f, 0.5f, -1.5f ), vec3f( 0.5f, 0.5f, 0.5f ) );
	}

	// Getters for settings
	Visualization visualization() const noexcept
	{
		return _visualization;
	}
	Filtering filtering() const noexcept
	{
		return _filtering;
	}
	Compositing compositing() const noexcept
	{
		return _compositing;
	}
	vec4f shadingParams() const noexcept
	{
		return _shadingParams;
	}
	int32_t stepsPerVoxel() const noexcept
	{
		return _stepsPerVoxel;
	}
	const std::pair<vec3i, vec3i>& clipRegion() const noexcept
	{
		return _clipRegion;
	}
	const Camera& camera() const noexcept
	{
		return _camera;
	}
	bool showHighlightedRegion() const noexcept
	{
		return _showHighlightedRegion;
	}
	vec3f highlightedRegionColor() const noexcept
	{
		return _highlightedRegionColor;
	}
	vec3i slice() const noexcept
	{
		return _slice;
	}
	vec2i highlightedTexel() const noexcept
	{
		return _highlightedTexel;
	}

signals:
	// Signals for when setings change
	void visualizationChanged( Visualization );
	void filteringChanged( Filtering );
	void compositingChanged( Compositing );
	void shadingParamsChanged( vec4f );
	void stepsPerVoxelChanged( int32_t );
	void clipRegionChanged( const std::pair<vec3i, vec3i>& );
	void cameraChanged( const Camera& );
	void showHighlightedRegionChanged( bool );
	void highlightedRegionColorChanged( vec3f );
	void sliceChanged( vec3i );
	void highlightedTexelChanged( vec2i );

public slots:
	// Setters for settings
	void setVisualization( Visualization visualization )
	{
		if( visualization != _visualization )
			emit visualizationChanged( _visualization = visualization );
	}
	void setFiltering( Filtering filtering )
	{
		if( filtering != _filtering )
			emit filteringChanged( _filtering = filtering );
	}
	void setCompositing( Compositing compositing )
	{
		if( compositing != _compositing )
			emit compositingChanged( _compositing = compositing );
	}
	void setShadingParams( vec4f shadingParams )
	{
		if( shadingParams != _shadingParams )
			emit shadingParamsChanged( _shadingParams = shadingParams );
	}
	void setStepsPerVoxel( int32_t stepsPerVoxel )
	{
		if( stepsPerVoxel != _stepsPerVoxel )
			emit stepsPerVoxelChanged( _stepsPerVoxel = stepsPerVoxel );
	}
	void setClipRegion( const std::pair<vec3i, vec3i>& clipRegion )
	{
		if( clipRegion != _clipRegion )
			emit clipRegionChanged( _clipRegion = clipRegion );
	}
	void setCamera( vec3f position, vec3f lookAt )
	{
		if( position != _camera.position || lookAt != _camera.lookAt )
		{
			_camera.position = position;
			_camera.lookAt = lookAt;

			_camera.forward = ( _camera.lookAt - _camera.position ).normalized();
			_camera.right = vec3f( 0.0f, 1.0f, 0.0f ).cross( _camera.forward ).normalized();
			_camera.up = _camera.forward.cross( _camera.right ).normalized();

			emit cameraChanged( _camera );
		}
	}
	void setShowHighlightedRegion( bool show )
	{
		if( _showHighlightedRegion != show )
			emit showHighlightedRegionChanged( _showHighlightedRegion = show );
	}
	void setHighlightedRegionColor( QColor color )
	{
		emit highlightedRegionColorChanged( _highlightedRegionColor = vec3f( color.red(), color.green(), color.blue() ) / 255.0f );
	}
	void setSlice( vec3i slice )
	{
		if( slice != _slice )
			emit sliceChanged( _slice = slice );
	}
	void setHighlightedTexel( vec2i texel )
	{
		if( texel != _highlightedTexel )
			emit highlightedTexelChanged( _highlightedTexel = texel );
	}

private:
	Visualization _visualization = Visualization::e3D;
	Filtering _filtering = Filtering::eLinear;
	Compositing _compositing = Compositing::eAlphaBlending;
	vec4f _shadingParams = vec4f( 0.3f, 0.3f, 0.4f, 10.0f );
	int32_t _stepsPerVoxel = 3;
	std::pair<vec3i, vec3i> _clipRegion;
	Camera _camera;
	bool _showHighlightedRegion = false;
	vec3f _highlightedRegionColor;

	vec3i _slice = vec3i( -1, 0, -1 );
	vec2i _highlightedTexel = vec2i( -1, -1 );
};

// Class to manage the representation of region as used by a volume renderer
class VolumeRendererRegion : public QObject
{
	Q_OBJECT
public:
	// Constructor :)
	VolumeRendererRegion( QObject* parent, QString name ) : QObject( parent ), _name( std::move( name ) )
	{}

	// Setter for the region's name
	void setName( QString name )
	{
		_name = std::move( name );
		emit nameChanged();
	}

	// Setters for the mask and the input volumes
	void setMask( std::shared_ptr<Volume<float>> mask )
	{
		_maskVolume = mask;
		emit maskChanged();
		emit regionChanged();
	}
	void setFirstVolume( const Volume<float>* volume, QString field )
	{
		_volumes.first = volume;
		_firstVolumeField = std::move( field );
		emit regionChanged();
	}
	void setSecondVolume( const Volume<float>* volume, QString field )
	{
		_volumes.second = volume;
		_secondVolumeField = std::move( field );
		emit regionChanged();
	}
	void setAlphaVolume( const Volume<float>* volume, QString field )
	{
		_alphaVolume = volume;
		_alphaVolumeField = std::move( field );
		emit regionChanged();
	}

	// Setters for the color maps
	void setColorMap( const ColorMap1D* colorMap )
	{
		if( _colorMap1D && _colorMap1DAlpha != _colorMap1D ) QObject::disconnect( _colorMap1D, nullptr, this, nullptr );
		if( ( _colorMap1D = colorMap ) && _colorMap1DAlpha != _colorMap1D )
			QObject::connect( _colorMap1D, &ColorMap1D::colorMapChanged, this, &VolumeRendererRegion::regionChanged );
		emit regionChanged();
	}
	void setColorMap2D( const ColorMap2D* colorMap )
	{
		if( _colorMap2D ) QObject::disconnect( _colorMap2D, nullptr, this, nullptr );
		if( _colorMap2D = colorMap ) QObject::connect( _colorMap2D, &ColorMap2D::colorMapChanged, this, &VolumeRendererRegion::regionChanged );
		emit regionChanged();
	}
	void setColorMapAlpha( const ColorMap1D* colorMap )
	{
		if( _colorMap1DAlpha && _colorMap1DAlpha != _colorMap1D ) QObject::disconnect( _colorMap1DAlpha, nullptr, this, nullptr );
		if( ( _colorMap1DAlpha = colorMap ) && _colorMap1DAlpha != _colorMap1D )
			QObject::connect( _colorMap1DAlpha, &ColorMap1D::colorMapChanged, this, &VolumeRendererRegion::regionChanged );
		emit regionChanged();
	}

	// Getters for the mask and the input volumes
	std::shared_ptr<Volume<float>> mask() const noexcept
	{
		return _maskVolume;
	}
	std::pair<const Volume<float>*, const Volume<float>*> volumes() const noexcept
	{
		return _volumes;
	}
	const Volume<float>* alphaVolume() const noexcept
	{
		return _alphaVolume;
	}

	// Getters for the used color maps
	const ColorMap1D* colorMap1D() const noexcept
	{
		return _colorMap1D;
	}
	const ColorMap2D* colorMap2D() const noexcept
	{
		return _colorMap2D;
	}
	const ColorMap1D* colorMap1DAlpha() const noexcept
	{
		return _colorMap1DAlpha;
	}

	// Getters for the texture of the mask and the input volumes
	GLuint maskTexture() const noexcept
	{
		return _maskVolume ? _maskVolume->texture() : 0;
	}
	GLuint firstVolumeTexture() const noexcept
	{
		return _volumes.first ? _volumes.first->texture() : 0;
	}
	GLuint secondVolumeTexture() const noexcept
	{
		return _volumes.second ? _volumes.second->texture() : 0;
	}
	GLuint alphaVolumeTexture() const noexcept
	{
		return _alphaVolume ? _alphaVolume->texture() : 0;
	}

	// Getter for the name of the regions
	const QString& name() const noexcept
	{
		return _name;
	}

	// Getters for the names of fields for the different input volumes
	const QString& firstVolumeField() const noexcept
	{
		return _firstVolumeField;
	}
	const QString& secondVolumeField() const noexcept
	{
		return _secondVolumeField;
	}
	const QString& alphaVolumeField() const noexcept
	{
		return _alphaVolumeField;
	}

signals:
	// Signals for whe different properties change
	void nameChanged();
	void maskChanged();
	void regionChanged();

private:
	QString _name;
	std::shared_ptr<Volume<float>> _maskVolume;
	std::pair<const Volume<float>*, const Volume<float>*> _volumes;
	const Volume<float>* _alphaVolume = nullptr;

	QString _firstVolumeField, _secondVolumeField, _alphaVolumeField;

	const ColorMap1D* _colorMap1D = nullptr;
	const ColorMap2D* _colorMap2D = nullptr;
	const ColorMap1D* _colorMap1DAlpha = nullptr;
};


// Class for the text overlay used in the slice viewer (NOTE: Qt has problems using the QPainter and OpenGL simultaneously in the same class, thus the QPainter part is outsourced to this class)
class SliceOverlay : public QWidget
{
	Q_OBJECT
public:
	// Struct for region information at the current voxel
	struct Region
	{
		// Struct for input volume information at the current voxel
		struct Volume
		{
			QString name; // Volume name
			double value = 0.0; // Value of the voxel inside the volume
		};

		QString name; // Region name
		std::array<Volume, 3> volumes; // Input volumes
		vec4f color; // Output color for this region at the current voxel
	};

	// Constructor :)
	SliceOverlay() : QWidget()
	{
		this->setStyleSheet( "background:transparent" );
		this->setMouseTracking( true );
	}

	// Setter for the dimension of the input volumes
	void setDimensions( vec3i dimensions )
	{
		_dimensions = dimensions;
		this->updateTarget();
	}

	// Setter for the current slice
	void setSlice( vec3i slice )
	{
		_slice = slice;
		this->updateTarget();
	}

	// Setter for the region information at the current voxel and the region that is used to color the voxel
	void setRegions( std::vector<Region> regions, int32_t chosenRegion )
	{
		_regions = std::move( regions );
		_chosenRegion = chosenRegion;
		this->update();
	}

	// Setter and getter for the hovered texel (converted to voxel later)
	void setHoveredTexel( vec2i texel )
	{
		if( _hoveredTexel != texel )
			emit hoveredTexelChanged( _hoveredTexel = texel );
	}
	vec2i hoveredTexel() const noexcept
	{
		return _hoveredTexel;
	}

signals:
	// Signals when the hovered texel changes
	void hoveredTexelChanged( vec2i );

private:
	// Handle resize events
	void resizeEvent( QResizeEvent* event ) override
	{
		QWidget::resizeEvent( event );
		this->updateTarget();
	}

	// Draw the current slice and region information at the current voxel
	void paintEvent( QPaintEvent* event ) override
	{
		// Draw outline/border of whole slice
		auto painter = QPainter( this );
		painter.setRenderHint( QPainter::Antialiasing, true );

		auto font = this->font();
		font.setPointSize( 14 );
		painter.setFont( font );

		painter.setPen( Qt::black );
		painter.setBrush( Qt::transparent );
		painter.drawRect( _target );

		// Draw outline of hovered texel
		auto valignment = Qt::AlignTop;
		if( _hoveredTexel != vec2i( -1, -1 ) )
		{
			const auto xstep = _target.width() / _textureSize.x;
			const auto ystep = _target.height() / _textureSize.y;
			const auto texelRect = QRect( _target.left() + xstep * _hoveredTexel.x, _target.bottom() - ystep * _hoveredTexel.y, xstep, -ystep );
			painter.drawRect( texelRect );

			if( texelRect.center().y() < this->rect().center().y() ) valignment = Qt::AlignBottom;
		}

		// Draw current slice or exact voxel position when hovering a voxel
		const auto rect = this->rect().marginsRemoved( QMargins( 10, 10, 10, 10 ) );

		auto text = QString();
		if( _hoveredTexel != vec2i( -1, -1 ) )
		{
			auto voxel = vec3i();
			voxel.x = _slice.x == -1 ? _hoveredTexel.x : _slice.x;
			voxel.y = _slice.x == -1 ? ( _slice.y == -1 ? _hoveredTexel.y : _slice.y ) : ( _slice.y == -1 ? _hoveredTexel.x : _slice.y );
			voxel.z = _slice.z == -1 ? _hoveredTexel.y : _slice.z;
			text = "Voxel: xyz(" + QString::number( voxel.x ) + ", " + QString::number( voxel.y ) + ", " + QString::number( voxel.z ) + ")";
		}
		else
		{
			text = QString( "Slice: " );
			const auto dimensionChars = std::array<QString, 3> { "X", "Y", "Z" };
			for( int32_t i = 0; i < 3; ++i ) if( _slice[i] != -1 ) text += dimensionChars[i] + " = " + QString::number( _slice[i] );
		}

		// Draw region information for the hovered voxel
		if( _hoveredTexel != vec2i( -1, -1 ) ) for( const auto& region : _regions )
		{
			text += "\n\n" + region.name + ": rgba(" + QString::number( region.color.x, 'f', 2 ) + ", " + QString::number( region.color.y, 'f', 2 ) +
				", " + QString::number( region.color.z, 'f', 2 ) + ", " + QString::number( region.color.w, 'f', 2 ) + ")";
			for( const auto& volume : region.volumes ) if( !volume.name.isEmpty() )
				text += "\n" + volume.name + ": " + QString::number( volume.value, 'f', 3 );
		}
		painter.drawText( rect, valignment | Qt::AlignLeft, text );
	}

	// Handle mouse move events
	void mouseMoveEvent( QMouseEvent* event ) override
	{
		if( event->buttons() == Qt::NoButton )
			this->updateHoveredTexel( event->pos() );
	}

	// Reset the hovered texel when the mouse leaves
	void leaveEvent( QEvent* event ) override
	{
		emit hoveredTexelChanged( _hoveredTexel = vec2i( -1, -1 ) );
		this->update();
	}

	// Update outline of slice
	void updateTarget()
	{
		_textureSize = vec2i( 0, 0 );
		if( _slice.x == -1 ) ( _textureSize.x ? _textureSize.y : _textureSize.x ) = _dimensions.x;
		if( _slice.y == -1 ) ( _textureSize.x ? _textureSize.y : _textureSize.x ) = _dimensions.y;
		if( _slice.z == -1 ) ( _textureSize.x ? _textureSize.y : _textureSize.x ) = _dimensions.z;

		const auto scale = std::min( this->width() / _textureSize.x, this->height() / _textureSize.y );
		_target = QRect( 0, 0, _textureSize.x * scale, _textureSize.y * scale );
		_target.moveCenter( this->rect().center() );
		this->update();
	}

	// Update the hovered texel
	void updateHoveredTexel( QPoint point )
	{
		const auto x = static_cast<double>( point.x() - _target.left() ) / _target.width();
		const auto y = static_cast<double>( _target.bottom() - point.y() ) / _target.height();

		auto hoveredTexel = vec2i( -1, -1 );
		if( x >= 0.0 && x <= 1.0 && y >= 0.0 && y <= 1.0 )
		{
			hoveredTexel = vec2d( x, y ) * _textureSize;
			hoveredTexel.x = std::clamp( hoveredTexel.x, 0, _textureSize.x - 1 );
			hoveredTexel.y = std::clamp( hoveredTexel.y, 0, _textureSize.y - 1 );
		}

		if( hoveredTexel != _hoveredTexel )
			emit hoveredTexelChanged( _hoveredTexel = hoveredTexel );

		this->update();
	}

	vec3i _dimensions = vec3i( 1, 1, 1 );
	vec3i _slice = vec3i( -1, 0, -1 );
	std::vector<Region> _regions;
	int32_t _chosenRegion = -1;

	vec2i _textureSize = vec2i( 0, 0 );
	QRect _target;
	vec2i _hoveredTexel = vec2i( -1, -1 );
};

// Class for volume rendering using ray casting
class VolumeRenderer :public QOpenGLWidget, public QOpenGLFunctions_4_5_Core
{
	Q_OBJECT
public:
	// Constructor :)
	VolumeRenderer( VolumeRendererSettings& settings, QWidget* parent ) : QOpenGLWidget( parent ), QOpenGLFunctions_4_5_Core(), _settings( settings )
	{
		this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );
		this->setFocusPolicy( Qt::FocusPolicy::WheelFocus );

		// Initialize slice overlay
		auto layout = new QVBoxLayout( this );
		layout->setContentsMargins( 0, 0, 0, 0 );
		layout->addWidget( _sliceOverlay = new SliceOverlay );
		_sliceOverlay->setVisible( false );

		// Initialize connections
		const auto update = QOverload<>::of( &QWidget::update );
		QObject::connect( &settings, &VolumeRendererSettings::visualizationChanged, this, &VolumeRenderer::updateSlice );
		QObject::connect( &settings, &VolumeRendererSettings::filteringChanged, this, &VolumeRenderer::updateFiltering );
		QObject::connect( &settings, &VolumeRendererSettings::compositingChanged, this, update );
		QObject::connect( &settings, &VolumeRendererSettings::shadingParamsChanged, this, update );
		QObject::connect( &settings, &VolumeRendererSettings::stepsPerVoxelChanged, this, update );
		QObject::connect( &settings, &VolumeRendererSettings::clipRegionChanged, this, update );
		QObject::connect( &settings, &VolumeRendererSettings::cameraChanged, this, update );
		QObject::connect( &settings, &VolumeRendererSettings::showHighlightedRegionChanged, this, update );
		// QObject::connect( &settings, &VolumeRendererSettings::highlightedRegionColorChanged, this, update );
		QObject::connect( &settings, &VolumeRendererSettings::sliceChanged, this, &VolumeRenderer::updateSlice );
		QObject::connect( &settings, &VolumeRendererSettings::highlightedTexelChanged, this, &VolumeRenderer::updateSlice );
		QObject::connect( _sliceOverlay, &SliceOverlay::hoveredTexelChanged, [=] ( vec2i texel )
		{
			_settings.setHighlightedTexel( texel );
		} );
	}

	// Destroy OpenGL objects
	virtual ~VolumeRenderer()
	{
		this->makeCurrent();
		glDeleteTextures( 1, &_sliceTexture );
		glDeleteSamplers( 1, &_sampler );
		glDeleteSamplers( 1, &_samplerColorMaps );
	}

	// Add or remove a volume renderer region
	void addRegion( int32_t index, QString name )
	{
		auto region = new VolumeRendererRegion( this, std::move( name ) );
		QObject::connect( region, &VolumeRendererRegion::regionChanged, this, &VolumeRenderer::onRegionChanged );
		_regions.insert( _regions.begin() + index, region );

		this->onRegionChanged();
	}
	void removeRegion( int32_t index )
	{
		if( _regions.size() > 1 )
		{
			QObject::disconnect( _regions[index], nullptr, this, nullptr );
			_regions[index]->deleteLater();
			_regions.erase( _regions.begin() + index );
			this->onRegionChanged();
		}
	}

	// Setter for the name of a region (relevant for the slice viewer)
	void setRegionName( int32_t index, QString name )
	{
		_regions[index]->setName( std::move( name ) );
		this->onRegionChanged();
	}

	// Set the number of regions to a fixed number
	void setRegionCount( const std::vector<QString> names )
	{
		for( int32_t i = 0; i < _regions.size(); ++i )
		{
			QObject::disconnect( _regions[i], nullptr, this, nullptr );
			_regions[i]->deleteLater();
		}

		_regions.clear();
		for( int32_t i = 0; i < names.size(); ++i ) this->addRegion( 0, names[i] );
	}

	// Swap two regions (changing their priority)
	void swapRegions( int32_t first, int32_t second )
	{
		if( first > second ) std::swap( first, second );
		std::swap( _regions[first], _regions[second] );
		this->onRegionChanged();
	}

	// Getter for a volume renderer region
	VolumeRendererRegion* region( int32_t index )
	{
		return _regions[index];
	}

	// Setter for the highlighted region (used for region brushing)
	void setHighlightedRegion( std::shared_ptr<Volume<float>> region )
	{
		_highlightedRegion = region;
		this->update();
	}

signals:
	// Signals when the volume renderer finished initializing its OpenGL state
	void initialized();

	// Signals when the highlighted region changed (through brushing)
	void highlightedRegionChanged( std::shared_ptr<Volume<float>> );

	// Signals that the input ensemble of this volume renderer should be used through the application (parallel coordinates, color map editors etc.)
	void requestEnsembleUsage();

private slots:
	// Update the filtering mode
	void updateFiltering( Filtering filtering )
	{
		GLint param = GL_LINEAR;
		if( filtering == Filtering::eNearest ) param = GL_NEAREST;
		else if( filtering == Filtering::eLinear ) param = GL_LINEAR;

		this->makeCurrent();
		glSamplerParameteri( _sampler, GL_TEXTURE_MAG_FILTER, param );
		glSamplerParameteri( _sampler, GL_TEXTURE_MIN_FILTER, param );

		this->update();
	}

	// Update the slice viewer
	void updateSlice()
	{
		if( !_initialized || !_regions.front()->volumes().first ) return;
		this->update();

		// Update the visibility
		_sliceOverlay->setVisible( _settings.visualization() == VolumeRendererSettings::Visualization::eSlice );
		if( _settings.visualization() != VolumeRendererSettings::Visualization::eSlice ) return;

		// Struct that stores the values at the requested voxel
		struct PixelQuery
		{
			vec4f colors[10];
			float values[30] {};
			int32_t chosenRegion = 0;
		} pixelQuery;

		this->makeCurrent();

		// First-time setup
		if( !_sliceTexture )
		{
			glGenTextures( 1, &_sliceTexture );
			glBindTexture( GL_TEXTURE_2D, _sliceTexture );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );

			glGenBuffers( 1, &_pixelQueryBuffer );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, _pixelQueryBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( PixelQuery ), nullptr, GL_DYNAMIC_READ );
		}

		// Calculate dimension and location of slice
		const auto dimensions = _regions.front()->volumes().first->dimensions();
		const auto slice = _settings.slice();
		const auto pixelQueryTexel = _settings.highlightedTexel();

		int width = 0, height = 0;
		if( slice.x == -1 ) ( width ? height : width ) = dimensions.x;
		if( slice.y == -1 ) ( width ? height : width ) = dimensions.y;
		if( slice.z == -1 ) ( width ? height : width ) = dimensions.z;

		if( _renderDoc && _captureFrame ) _renderDoc->StartFrameCapture( nullptr, nullptr );
		glBindTexture( GL_TEXTURE_2D, _sliceTexture );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr );

		// Render the specified slice and gather the values at the requested voxel
		_shaderSliceRenderer.bind();

		auto texture = GL_TEXTURE1;
		for( int32_t i = 0; i < _regions.size(); ++i )
		{
			const auto region = _regions[i];

			const auto colorMap1D = region->colorMap1D();
			const auto colorMap2D = region->colorMap2D();
			const auto colorMap1DAlpha = region->colorMap1DAlpha();

			const auto firstRange = colorMap2D ? colorMap2D->firstDomain() : colorMap1D ? colorMap1D->domain() : vec2d( 0.0, 1.0 );
			const auto secondRange = colorMap2D ? colorMap2D->secondDomain() : vec2d( 0.0, 1.0 );
			const auto thirdRange = colorMap1DAlpha ? colorMap1DAlpha->domain() : vec2d( 0.0, 1.0 );

			const auto regionText = "regions[" + std::to_string( i ) + "].";
			auto index = texture - GL_TEXTURE0;
			_shaderSliceRenderer.setUniformValue( ( regionText + "mask" ).data(), index + 1 );
			_shaderSliceRenderer.setUniformValue( ( regionText + "volumes[0]" ).data(), index + 2 );
			_shaderSliceRenderer.setUniformValue( ( regionText + "volumes[1]" ).data(), index + 3 );
			_shaderSliceRenderer.setUniformValue( ( regionText + "volumes[2]" ).data(), index + 4 );
			_shaderSliceRenderer.setUniformValue( ( regionText + "colorMap1D" ).data(), index + 5 );
			_shaderSliceRenderer.setUniformValue( ( regionText + "colorMap2D" ).data(), index + 6 );
			_shaderSliceRenderer.setUniformValue( ( regionText + "colorMap1DAlpha" ).data(), index + 7 );
			_shaderSliceRenderer.setUniformValue( ( regionText + "ranges[0]" ).data(), firstRange.x, firstRange.y );
			_shaderSliceRenderer.setUniformValue( ( regionText + "ranges[1]" ).data(), secondRange.x, secondRange.y );
			_shaderSliceRenderer.setUniformValue( ( regionText + "ranges[2]" ).data(), thirdRange.x, thirdRange.y );
			_shaderSliceRenderer.setUniformValue( ( regionText + "useColorMap2D" ).data(), !!colorMap2D );
			_shaderSliceRenderer.setUniformValue( ( regionText + "useColorMap1DAlpha" ).data(), !!colorMap1DAlpha );

			glActiveTexture( ++texture );
			glBindTexture( GL_TEXTURE_3D, region->maskTexture() );
			glBindSampler( texture - GL_TEXTURE0, _samplerColorMaps );

			glActiveTexture( ++texture );
			glBindTexture( GL_TEXTURE_3D, region->firstVolumeTexture() );
			glBindSampler( texture - GL_TEXTURE0, _sampler );

			glActiveTexture( ++texture );
			glBindTexture( GL_TEXTURE_3D, region->secondVolumeTexture() );
			glBindSampler( texture - GL_TEXTURE0, _sampler );

			glActiveTexture( ++texture );
			glBindTexture( GL_TEXTURE_3D, region->alphaVolumeTexture() );
			glBindSampler( texture - GL_TEXTURE0, _sampler );

			glActiveTexture( ++texture );
			glBindTexture( GL_TEXTURE_1D, colorMap1D ? colorMap1D->texture() : 0 );
			glBindSampler( texture - GL_TEXTURE0, _samplerColorMaps );

			glActiveTexture( ++texture );
			glBindTexture( GL_TEXTURE_2D, colorMap2D ? colorMap2D->texture() : 0 );
			glBindSampler( texture - GL_TEXTURE0, _samplerColorMaps );

			glActiveTexture( ++texture );
			glBindTexture( GL_TEXTURE_1D, colorMap1DAlpha ? colorMap1DAlpha->texture() : 0 );
			glBindSampler( texture - GL_TEXTURE0, _samplerColorMaps );
		}

		_shaderSliceRenderer.setUniformValue( "regionCount", static_cast<int32_t>( _regions.size() ) );
		_shaderSliceRenderer.setUniformValue( "slice", static_cast<float>( slice.x ), static_cast<float>( slice.y ), static_cast<float>( slice.z ) );
		_shaderSliceRenderer.setUniformValue( "pixelQueryTexel", static_cast<float>( pixelQueryTexel.x ), static_cast<float>( pixelQueryTexel.y ) );

		glBindImageTexture( 0, _sliceTexture, 0, false, 0, GL_WRITE_ONLY, GL_RGBA32F );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, _pixelQueryBuffer );
		glDispatchCompute( width, height, 1 );

		// Gather region information for slice viewer from pixel query buffer
		auto regions = std::vector<SliceOverlay::Region>( _regions.size() );
		glGetBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, sizeof( PixelQuery ), &pixelQuery );
		for( int32_t i = 0; i < regions.size(); ++i )
		{
			regions[i].name = _regions[i]->name();
			regions[i].color = pixelQuery.colors[i];

			if( _regions[i]->volumes().first )
			{
				regions[i].volumes[0].name = _regions[i]->volumes().first->name();
				if( !_regions[i]->firstVolumeField().isEmpty() ) regions[i].volumes[0].name += " (" + _regions[i]->firstVolumeField() + ")";
			}
			if( _regions[i]->colorMap2D() && _regions[i]->volumes().second )
			{
				regions[i].volumes[1].name = _regions[i]->volumes().second->name();
				if( !_regions[i]->secondVolumeField().isEmpty() ) regions[i].volumes[1].name += " (" + _regions[i]->secondVolumeField() + ")";
			}
			if( _regions[i]->colorMap1DAlpha() && _regions[i]->alphaVolume() )
			{
				regions[i].volumes[2].name = _regions[i]->alphaVolume()->name();
				if( !_regions[i]->alphaVolumeField().isEmpty() ) regions[i].volumes[2].name += " (" + _regions[i]->alphaVolumeField() + ")";
			}

			for( int32_t j = 0; j < 3; ++j )									regions[i].volumes[j].value = pixelQuery.values[3 * i + j];
		}

		// Update values of slice viewer
		_sliceOverlay->setRegions( std::move( regions ), pixelQuery.chosenRegion );
		_sliceOverlay->setHoveredTexel( pixelQueryTexel );
		_sliceOverlay->setDimensions( dimensions );
		_sliceOverlay->setSlice( slice );

		if( _renderDoc ) _renderDoc->EndFrameCapture( nullptr, nullptr ), _captureFrame = false;
	}

	// Handle the change of a region
	void onRegionChanged()
	{
		if( _settings.visualization() == VolumeRendererSettings::Visualization::eSlice ) this->updateSlice();
		else this->update();
	}

private:
	// Initialize OpenGL objects
	void initializeGL() override
	{
		if( HMODULE module = GetModuleHandleA( "renderdoc.dll" ) )
		{
			pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI) GetProcAddress( module, "RENDERDOC_GetAPI" );
			RENDERDOC_GetAPI( eRENDERDOC_API_Version_1_1_2, (void**) &_renderDoc );
		}

		QOpenGLFunctions_4_5_Core::initializeOpenGLFunctions();

		// Compile shaders
		_shaderRaycaster.addShaderFromSourceFile( QOpenGLShader::Vertex, ":/shaders/fullscreen.vert" );
		_shaderRaycaster.addShaderFromSourceFile( QOpenGLShader::Fragment, ":/shaders/raycaster.frag" );
		_shaderRaycaster.link();

		_shaderPolygon.addShaderFromSourceCode( QOpenGLShader::Vertex,
			"#version 450\n"

			"layout( binding = 0 ) restrict readonly buffer BufferPoints { vec2 points[]; };"

			"void main() {"
			"	gl_Position = vec4( points[gl_VertexID], 0.0, 1.0 );"
			"}"
		);
		_shaderPolygon.addShaderFromSourceCode( QOpenGLShader::Fragment,
			"#version 450\n"

			"uniform vec4 color;"

			"layout( location = 0 ) out vec4 outColor;"

			"void main() {"
			"	outColor = color;"
			"}"
		);
		_shaderPolygon.link();

		_shaderBlendTexture.addShaderFromSourceCode( QOpenGLShader::Vertex,
			"#version 450\n"

			"layout( location = 0 ) out vec2 outTextureCoords;"

			"void main() {"
			"	vec2 positions[4]		= vec2[4]( vec2( -1.0, 1.0 ), vec2( -1.0, -1.0 ), vec2( 1.0, 1.0 ), vec2( 1.0, -1.0 ) );"
			"	vec2 textureCoords[4]	= vec2[4]( vec2( 0.0, 1.0 ), vec2( 0.0, 0.0 ), vec2( 1.0, 1.0 ), vec2( 1.0, 0.0 ) );"

			"	outTextureCoords = textureCoords[gl_VertexID];"
			"	gl_Position = vec4( positions[gl_VertexID], 0.0, 1.0 );"
			"}"
		);
		_shaderBlendTexture.addShaderFromSourceCode( QOpenGLShader::Fragment,
			"#version 450\n"

			"layout( location = 0 ) in vec2 inTextureCoords;"
			"uniform sampler2D inTexture;"
			"uniform bool useColor;"
			"uniform vec4 color;"

			"layout( location = 0 ) out vec4 outColor;"

			"void main() {"
			"	vec4 texel = texture( inTexture, inTextureCoords );"
			"	outColor = useColor? ( texel.x != 0.0? color : vec4( 0.0 ) ) : texel;"
			"}"
		);
		_shaderBlendTexture.link();

		_shaderRegionSelection.addShaderFromSourceFile( QOpenGLShader::Compute, ":/shaders/region_select.comp" );
		_shaderRegionSelection.link();

		_shaderSliceRenderer.addShaderFromSourceFile( QOpenGLShader::Compute, ":/shaders/slice_renderer.comp" );
		_shaderSliceRenderer.link();

		// Create and initialize samplers
		glGenSamplers( 1, &_sampler );
		glSamplerParameteri( _sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glSamplerParameteri( _sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glSamplerParameteri( _sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glSamplerParameteri( _sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glSamplerParameteri( _sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );

		glGenSamplers( 1, &_samplerColorMaps );
		glSamplerParameteri( _samplerColorMaps, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glSamplerParameteri( _samplerColorMaps, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glSamplerParameteri( _samplerColorMaps, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glSamplerParameteri( _samplerColorMaps, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glSamplerParameteri( _samplerColorMaps, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );

		// Update filtering and slice viewer
		_initialized = true;
		this->updateFiltering( _settings.filtering() );
		this->updateSlice();
		emit initialized();
	}

	// Handle resize event
	void resizeGL( int width, int height )
	{
		auto format = QOpenGLFramebufferObjectFormat();
		format.setInternalTextureFormat( GL_R16F );
		_framebuffer.reset( new QOpenGLFramebufferObject( width, height, format ) );
	}

	// Render volume or slice
	void paintGL() override
	{
		if( _settings.visualization() == VolumeRendererSettings::Visualization::e3D )
		{
			// Clear background if there are no available regions
			if( !_regions.front()->volumes().first )
			{
				glClearColor( 0.9f, 0.9f, 0.9f, 1.0f );
				glClear( GL_COLOR_BUFFER_BIT );
				return;
			}

			if( _captureFrame && _renderDoc ) _renderDoc->StartFrameCapture( nullptr, nullptr );

			// Prepare ray caster by updating uniforms
			const auto dimensions = _regions.front()->volumes().first->dimensions();
			const auto clipRangeBegin = vec3f( _settings.clipRegion().first ) / dimensions;
			const auto clipRangeEnd =  vec3f( _settings.clipRegion().second ) / dimensions;
			const auto maxDimension = std::max( dimensions.x, std::max( dimensions.y, dimensions.z ) );
			const auto dimensionScaling = maxDimension / vec3f( dimensions );

			const auto& camera = _settings.camera();
			const auto shadingParams = _settings.shadingParams();

			_shaderRaycaster.bind();
			_shaderRaycaster.setUniformValue( "viewport", static_cast<float>( this->width() ), static_cast<float>( this->height() ) );
			_shaderRaycaster.setUniformValue( "camera.pos", camera.position.x, camera.position.y, camera.position.z );
			_shaderRaycaster.setUniformValue( "camera.forward", camera.forward.x, camera.forward.y, camera.forward.z );
			if( this->width() > this->height() )
			{
				const auto correctedRight = camera.right * static_cast<float>( this->width() ) / this->height();
				_shaderRaycaster.setUniformValue( "camera.right", correctedRight.x, correctedRight.y, correctedRight.z );
				_shaderRaycaster.setUniformValue( "camera.up", camera.up.x, camera.up.y, camera.up.z );
			}
			else
			{
				const auto correctedUp = camera.up * static_cast<float>( this->height() ) / this->width();
				_shaderRaycaster.setUniformValue( "camera.right", camera.right.x, camera.right.y, camera.right.z );
				_shaderRaycaster.setUniformValue( "camera.up", correctedUp.x, correctedUp.y, correctedUp.z );
			}
			_shaderRaycaster.setUniformValue( "stepsize", 1.0f / ( maxDimension * _settings.stepsPerVoxel() ) );
			_shaderRaycaster.setUniformValue( "clipRegion[0]", clipRangeBegin.x, clipRangeBegin.y, clipRangeBegin.z );
			_shaderRaycaster.setUniformValue( "clipRegion[1]", clipRangeEnd.x, clipRangeEnd.y, clipRangeEnd.z );
			_shaderRaycaster.setUniformValue( "dimensions", static_cast<float>( dimensions.x ), static_cast<float>( dimensions.y ), static_cast<float>( dimensions.z ) );
			_shaderRaycaster.setUniformValue( "dimensionScaling", dimensionScaling.x, dimensionScaling.y, dimensionScaling.z );
			_shaderRaycaster.setUniformValue( "shadingParams", shadingParams.x, shadingParams.y, shadingParams.z, shadingParams.w );
			_shaderRaycaster.setUniformValue( "compositing", static_cast<GLint>( _settings.compositing() ) );
			_shaderRaycaster.setUniformValue( "gradientVolume", 0 );
			_shaderRaycaster.setUniformValue( "highlightedRegion", 1 );
			_shaderRaycaster.setUniformValue( "showHighlightedRegion", _settings.showHighlightedRegion() );
			_shaderRaycaster.setUniformValue( "highlightedRegionColor", _settings.highlightedRegionColor().x, _settings.highlightedRegionColor().y, _settings.highlightedRegionColor().z );

			glActiveTexture( GL_TEXTURE1 );
			glBindTexture( GL_TEXTURE_3D, _highlightedRegion ? _highlightedRegion->texture() : 0 );
			glBindSampler( 1, _samplerColorMaps );

			// Bind the required textures for all regions
			auto texture = GL_TEXTURE1;
			for( int32_t i = 0; i < _regions.size(); ++i )
			{
				const auto region = _regions[i];

				const auto colorMap1D = region->colorMap1D();
				const auto colorMap2D = region->colorMap2D();
				const auto colorMap1DAlpha = region->colorMap1DAlpha();

				const auto firstRange = colorMap2D ? colorMap2D->firstDomain() : colorMap1D ? colorMap1D->domain() : vec2d( 0.0, 1.0 );
				const auto secondRange = colorMap2D ? colorMap2D->secondDomain() : vec2d( 0.0, 1.0 );
				const auto thirdRange = colorMap1DAlpha ? colorMap1DAlpha->domain() : vec2d( 0.0, 1.0 );

				const auto regionText = "regions[" + std::to_string( i ) + "].";
				auto index = texture - GL_TEXTURE0;
				_shaderRaycaster.setUniformValue( ( regionText + "mask" ).data(), index + 1 );
				_shaderRaycaster.setUniformValue( ( regionText + "volumes[0]" ).data(), index + 2 );
				_shaderRaycaster.setUniformValue( ( regionText + "volumes[1]" ).data(), index + 3 );
				_shaderRaycaster.setUniformValue( ( regionText + "volumes[2]" ).data(), index + 4 );
				_shaderRaycaster.setUniformValue( ( regionText + "colorMap1D" ).data(), index + 5 );
				_shaderRaycaster.setUniformValue( ( regionText + "colorMap2D" ).data(), index + 6 );
				_shaderRaycaster.setUniformValue( ( regionText + "colorMap1DAlpha" ).data(), index + 7 );
				_shaderRaycaster.setUniformValue( ( regionText + "ranges[0]" ).data(), firstRange.x, firstRange.y );
				_shaderRaycaster.setUniformValue( ( regionText + "ranges[1]" ).data(), secondRange.x, secondRange.y );
				_shaderRaycaster.setUniformValue( ( regionText + "ranges[2]" ).data(), thirdRange.x, thirdRange.y );
				_shaderRaycaster.setUniformValue( ( regionText + "useColorMap2D" ).data(), !!colorMap2D );
				_shaderRaycaster.setUniformValue( ( regionText + "useColorMap1DAlpha" ).data(), !!colorMap1DAlpha );

				glActiveTexture( ++texture );
				glBindTexture( GL_TEXTURE_3D, region->maskTexture() );
				glBindSampler( texture - GL_TEXTURE0, _samplerColorMaps );

				glActiveTexture( ++texture );
				glBindTexture( GL_TEXTURE_3D, region->firstVolumeTexture() );
				glBindSampler( texture - GL_TEXTURE0, _sampler );

				glActiveTexture( ++texture );
				glBindTexture( GL_TEXTURE_3D, region->secondVolumeTexture() );
				glBindSampler( texture - GL_TEXTURE0, _sampler );

				glActiveTexture( ++texture );
				glBindTexture( GL_TEXTURE_3D, region->alphaVolumeTexture() );
				glBindSampler( texture - GL_TEXTURE0, _sampler );

				glActiveTexture( ++texture );
				glBindTexture( GL_TEXTURE_1D, colorMap1D ? colorMap1D->texture() : 0 );
				glBindSampler( texture - GL_TEXTURE0, _samplerColorMaps );

				glActiveTexture( ++texture );
				glBindTexture( GL_TEXTURE_2D, colorMap2D ? colorMap2D->texture() : 0 );
				glBindSampler( texture - GL_TEXTURE0, _samplerColorMaps );

				glActiveTexture( ++texture );
				glBindTexture( GL_TEXTURE_1D, colorMap1DAlpha ? colorMap1DAlpha->texture() : 0 );
				glBindSampler( texture - GL_TEXTURE0, _samplerColorMaps );
			}
			_shaderRaycaster.setUniformValue( "regionCount", static_cast<int32_t>( _regions.size() ) );

			// Render a fullscreen triangle to start ray caster
			glDrawArrays( GL_TRIANGLES, 0, 3 );

			// Render lasso for region brushing
			if( _lasso.size() )
			{
				_shaderBlendTexture.bind();
				_shaderBlendTexture.setUniformValue( "inTexture", texture - GL_TEXTURE0 + 1 );
				_shaderBlendTexture.setUniformValue( "useColor", true );
				_shaderBlendTexture.setUniformValue( "color", 1.0f, 1.0f, 1.0f, 0.5f );

				glEnable( GL_BLEND );
				glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE );

				glActiveTexture( ++texture );
				glBindTexture( GL_TEXTURE_2D, _framebuffer->textures()[0] );

				glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
				glDisable( GL_BLEND );
			}

			if( _captureFrame && _renderDoc ) _captureFrame = false, _renderDoc->EndFrameCapture( nullptr, nullptr );
		}
		else if( _settings.visualization() == VolumeRendererSettings::Visualization::eSlice )
		{
			// Clear background
			glClearColor( 0.9f, 0.9f, 0.9f, 1.0f );
			glClear( GL_COLOR_BUFFER_BIT );

			// Render slice texure in the center of the widget
			const auto dimensions = _regions.front()->volumes().first->dimensions();
			const auto slice = _settings.slice();

			int width = 0, height = 0;
			if( slice.x == -1 ) ( width ? height : width ) = dimensions.x;
			if( slice.y == -1 ) ( width ? height : width ) = dimensions.y;
			if( slice.z == -1 ) ( width ? height : width ) = dimensions.z;
			const auto scale = std::min( this->width() / width, this->height() / height );

			auto target = QRect( 0, 0, width * scale, height * scale );
			target.moveCenter( this->rect().center() );
			glViewport( target.x(), target.y(), target.width(), target.height() );

			_shaderBlendTexture.bind();
			_shaderBlendTexture.setUniformValue( "inTexture", 32 );
			_shaderBlendTexture.setUniformValue( "useColor", false );

			glEnable( GL_BLEND );
			glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO );

			glActiveTexture( GL_TEXTURE0 + 32 );
			glBindTexture( GL_TEXTURE_2D, _sliceTexture );

			glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
			glDisable( GL_BLEND );
		}
	}

	// Handle mouse press events
	void mousePressEvent( QMouseEvent* event ) override
	{
		// Update lasso or prepare to move camera
		if( _settings.showHighlightedRegion() ) _lasso.push_back( this->screenToPoint( event->pos() ) );
		else _previousPointDrag = event->localPos();
	}

	// Handle mouse move events
	void mouseMoveEvent( QMouseEvent* event ) override
	{
		if( _settings.showHighlightedRegion() )
		{
			// Add point to lasso
			_lasso.push_back( this->screenToPoint( event->pos() ) );

			// Upload points to lasso buffer
			this->makeCurrent();
			if( !_polygonBuffer ) glGenBuffers( 1, &_polygonBuffer );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, _polygonBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, _lasso.size() * sizeof( vec2f ), _lasso.data(), GL_STATIC_DRAW );

			// Render lasso
			_framebuffer->bind();
			glDrawBuffer( GL_COLOR_ATTACHMENT0 );
			glViewport( 0, 0, _framebuffer->width(), _framebuffer->height() );
			glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
			glClear( GL_COLOR_BUFFER_BIT );

			_shaderPolygon.bind();
			_shaderPolygon.setUniformValue( "color", 1.0f, 1.0f, 1.0f, 1.0f );

			glEnable( GL_BLEND );
			glBlendFunc( GL_ONE_MINUS_DST_COLOR, GL_ZERO );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, _polygonBuffer );
			glDrawArrays( GL_TRIANGLE_FAN, 0, _lasso.size() );
			glDisable( GL_BLEND );
		}
		else
		{
			const auto diff = event->localPos() - _previousPointDrag;
			const auto& camera = _settings.camera();

			// Shift camere on right-click, rotate on left-click
			if( event->buttons() == Qt::RightButton )
			{
				const auto shift = 0.005f * diff.y() * camera.up - 0.005f * diff.x() * camera.right;
				const auto position = camera.position + shift;
				const auto lookAt = camera.lookAt + shift;

				_settings.setCamera( position, lookAt );
			}
			else if( event->buttons() == Qt::LeftButton )
			{
				const auto relative = ( camera.position - camera.lookAt );
				const auto distance = relative.length();

				auto position = relative / distance;

				const auto rad = vec2f( -diff.x(), diff.y() ) / 180.0f * M_PI;

				// Rotate around (0.0, 1.0, 0.0)
				const auto cosH = std::cosf( rad.x );
				const auto sinH = std::sinf( rad.x );

				position.x = position.x * cosH - sinH * position.z;
				position.z = position.x * sinH + cosH * position.z;
				position = position.normalized();

				// Rotate around camera right vector
				const auto angle = position.dot( vec3f( 0.0f, 1.0f, 0.0f ) );
				if( ( rad.y > 0.0f && angle < 0.99f ) || ( rad.y < 0.0f && angle > -0.99f ) )
				{
					const auto cosV = std::cosf( rad.y );
					const auto sinV = std::sinf( rad.y );
					const auto [rx, ry, rz] = camera.right;

					position.x = position.dot( vec3f( cosV + rx * rx * ( 1 - cosV ), rx * ry * ( 1 - cosV ) - rz * sinV, rx * rz * ( 1 - cosV ) + ry * sinV ) );
					position.y = position.dot( vec3f( rx * ry * ( 1 - cosV ) + rz * sinV, cosV + ry * ry * ( 1 - cosV ), ry * rz * ( 1 - cosV ) - rx * sinV ) );
					position.z = position.dot( vec3f( rx * rz * ( 1 - cosV ) - ry * sinV, ry * rz * ( 1 - cosV ) + rx * sinV, cosV + rz * rz * ( 1 - cosV ) ) );
					position = position.normalized();
				}

				position = position.normalized() * distance + camera.lookAt;
				_settings.setCamera( position, camera.lookAt );
			}

			_previousPointDrag = event->localPos();
		}

		this->update();
	}

	// Handle mouse release events
	void mouseReleaseEvent( QMouseEvent* event ) override
	{
		if( _settings.showHighlightedRegion() )
		{
			// Prepare region selection compute shader
			const auto [x, y, z] = _highlightedRegion->dimensions();
			const auto maxDimension = std::max( x, std::max( y, z ) );
			const auto dimensionScaling = maxDimension / vec3f( x, y, z );
			const auto& camera = _settings.camera();
			const auto correctedUp = camera.up * static_cast<float>( this->height() ) / this->width();

			this->makeCurrent();
			_shaderRegionSelection.bind();
			_shaderRegionSelection.setUniformValue( "samplerPolygon", 32 );
			_shaderRegionSelection.setUniformValue( "dimensions", static_cast<float>( x ), static_cast<float>( y ), static_cast<float>( z ) );
			_shaderRegionSelection.setUniformValue( "dimensionScaling", dimensionScaling.x, dimensionScaling.y, dimensionScaling.z );
			_shaderRegionSelection.setUniformValue( "viewport", static_cast<float>( this->width() ), static_cast<float>( this->height() ) );
			_shaderRegionSelection.setUniformValue( "camera.pos", camera.position.x, camera.position.y, camera.position.z );
			_shaderRegionSelection.setUniformValue( "camera.forward", camera.forward.x, camera.forward.y, camera.forward.z );
			if( this->width() > this->height() )
			{
				const auto correctedRight = camera.right * static_cast<float>( this->width() ) / this->height();
				_shaderRegionSelection.setUniformValue( "camera.right", correctedRight.x, correctedRight.y, correctedRight.z );
				_shaderRegionSelection.setUniformValue( "camera.up", camera.up.x, camera.up.y, camera.up.z );
			}
			else
			{
				const auto correctedUp = camera.up * static_cast<float>( this->height() ) / this->width();
				_shaderRegionSelection.setUniformValue( "camera.right", camera.right.x, camera.right.y, camera.right.z );
				_shaderRegionSelection.setUniformValue( "camera.up", correctedUp.x, correctedUp.y, correctedUp.z );
			}
			_shaderRegionSelection.setUniformValue( "mode", event->button() == Qt::LeftButton ? 0 : event->button() == Qt::MiddleButton ? 1 : 2 );

			glActiveTexture( GL_TEXTURE0 + 32 );
			glBindTexture( GL_TEXTURE_2D, _framebuffer->textures()[0] );
			glBindImageTexture( 0, _highlightedRegion->texture(), 0, false, 0, GL_READ_WRITE, GL_R32F );
			glDispatchCompute( x, y, z );

			// Retrieve new selection of voxels and signal their change
			auto values = std::vector<float>( _highlightedRegion->voxelCount() );
			glBindTexture( GL_TEXTURE_3D, _highlightedRegion->texture() );
			glGetTexImage( GL_TEXTURE_3D, 0, GL_RED, GL_FLOAT, values.data() );

			_highlightedRegion->setValues( std::move( values ) );
			emit highlightedRegionChanged( _highlightedRegion );

			_lasso.clear();
			this->update();
		}
	}

	// Handle mouse wheel events
	void wheelEvent( QWheelEvent* event ) override
	{
		if( _settings.visualization() == VolumeRendererSettings::Visualization::e3D )
		{
			// Zoom the camera
			const auto& camera = _settings.camera();
			auto position = camera.position;

			if( event->delta() > 0 ) position += 0.05f * camera.forward;
			else position -= 0.05f * camera.forward;

			if( ( camera.position - camera.lookAt ).length() < 0.1f )
				position = camera.lookAt - 0.1f * camera.forward;

			_settings.setCamera( position, camera.lookAt );
		}
		else
		{
			auto slice = _settings.slice();

			// When holding shift, switch the viewing direction; otherwise iterate through slices
			if( event->modifiers() & Qt::ShiftModifier )
			{
				int32_t i = 0;
				for( ; i < 3; ++i ) if( slice[i] != -1 ) break;
				slice[i] = -1;
				slice[( i + ( event->delta() > 0 ? 1 : 2 ) ) % 3] = 0;
			}
			else
			{
				const auto dim = slice.x == -1 ? slice.y == -1 ? 2 : 1 : 0;
				slice[dim] = std::clamp( slice[dim] + ( event->delta() > 0 ? 1 : -1 ), 0, _regions.front()->volumes().first->dimensions()[dim] - 1 );
			}
			_settings.setSlice( slice );
		}
	}

	// Handle keyboard events
	void keyPressEvent( QKeyEvent* event ) override
	{
		if( event->key() == Qt::Key_C )
		{
			_captureFrame = true;
			if( _settings.visualization() == VolumeRendererSettings::Visualization::eSlice ) this->updateSlice();
			else this->update();
		}
		else if( event->key() == Qt::Key_E ) emit requestEnsembleUsage();
		else if( event->key() == Qt::Key_S )
		{
			if( _settings.visualization() == VolumeRendererSettings::Visualization::e3D ) _settings.setVisualization( VolumeRendererSettings::Visualization::eSlice );
			else if( _settings.visualization() == VolumeRendererSettings::Visualization::eSlice ) _settings.setVisualization( VolumeRendererSettings::Visualization::e3D );
		}
		else if( event->key() == Qt::Key_R ) _settings.setShowHighlightedRegion( !_settings.showHighlightedRegion() );
		else if( event->key() == Qt::Key_I && _settings.showHighlightedRegion() )
		{
			// Invert the current region (NOT operation)
			auto values = _highlightedRegion->values();
			util::compute_multi_threaded( 0, values.size(), [&] ( int32_t begin, int32_t end )
			{
				for( int32_t i = begin; i < end; ++i ) values[i] = !values[i];
			} );
			_highlightedRegion->setValues( std::move( values ) );
			emit highlightedRegionChanged( _highlightedRegion );

			this->makeCurrent();
			const auto [x, y, z] = _highlightedRegion->dimensions();
			glBindTexture( GL_TEXTURE_3D, _highlightedRegion->texture() );
			glTexImage3D( GL_TEXTURE_3D, 0, GL_R32F, x, y, z, 0, GL_RED, GL_FLOAT, _highlightedRegion->data() );

			this->update();
		}
	}

	// Helper functions to map a screen point to texture coordinates and vice versa
	vec2f screenToPoint( QPoint screen )
	{
		auto point = vec2f( screen.x(), screen.y() ) / vec2f( this->width(), this->height() ) * 2.0f - 1.0f;
		return vec2f( point.x, -point.y );
	}
	QPoint pointToScreen( vec2f screen )
	{
		auto point = ( vec2f( screen.x, -screen.y ) + 1.0f ) / 2.0f * vec2f( this->width(), this->height() );
		return QPoint( point.x, point.y );
	}

	RENDERDOC_API_1_4_1* _renderDoc = nullptr;
	bool _captureFrame = false;
	bool _initialized = false;

	VolumeRendererSettings& _settings;
	std::vector<VolumeRendererRegion*> _regions;
	SliceOverlay* _sliceOverlay = nullptr;

	std::shared_ptr<Volume<float>> _highlightedRegion;
	std::vector<vec2f> _lasso;

	QPointF _previousPointDrag;

	std::unique_ptr<QOpenGLFramebufferObject> _framebuffer;
	QOpenGLShaderProgram _shaderRaycaster, _shaderPolygon, _shaderBlendTexture, _shaderRegionSelection, _shaderSliceRenderer;
	GLuint _polygonBuffer = 0, _sliceTexture = 0;
	GLuint _sampler = 0, _samplerColorMaps = 0, _pixelQueryBuffer = 0;
};

// Class for managing a hierarchy of volume renderers
class VolumeRendererManager : public QWidget
{
	Q_OBJECT
public:
	// Proxy class to render the overlay when editing the volume renderer graph
	class Overlay : public QWidget
	{
	public:
		Overlay( QWidget* parent ) : QWidget( parent )
		{}

		QImage& image()
		{
			return _image;
		}

	private:
		void resizeEvent( QResizeEvent* event ) override
		{
			_image = QImage( this->width(), this->height(), QImage::Format_RGBA8888 );
		}
		void paintEvent( QPaintEvent* event ) override
		{
			auto painter = QPainter( this );
			painter.drawImage( this->rect(), _image );
		}
		QImage _image;
	};

	// Enum class for the interaction mode
	enum class InteractionMode { eViewing, eEditing };

	// Enum class for the types of links in the volume renderer graph
	enum class LinkType { eNone, eLeft, eRight, eSibling };

	// Struct for a grid cell in the volume renderer layout
	struct GridCell
	{
		int32_t row, col;
		GridCell( int32_t row = 0, int32_t col = 0 ) : row( row ), col( col ) {}

		bool operator==( GridCell other ) const noexcept
		{
			return row == other.row && col == other.col;
		}
		bool operator!=( GridCell other ) const noexcept
		{
			return row != other.row || col != other.col;
		}
	};

	// Struct that specified the position of a volume renderer inside the grid
	struct LayoutInfo
	{
		int32_t row = 0, col = 0, rowspan = 1, colspan = 1;

		LayoutInfo( int32_t row = 0, int32_t col = 0, int32_t rowspan = 1, int32_t colspan = 1 ) noexcept
			: row( row ), col( col ), rowspan( rowspan ), colspan( colspan )
		{}

		bool intersectsRow( int32_t row ) const noexcept
		{
			return row >= this->row && row < this->row + rowspan;
		}
		bool intersectsColumn( int32_t column ) const noexcept
		{
			return column >= this->col && column < this->col + colspan;
		}
		bool intersectsCell( GridCell cell ) const noexcept
		{
			return this->intersectsRow( cell.row ) && this->intersectsColumn( cell.col );
		}
		bool intersectsLayoutInfo( LayoutInfo info ) const noexcept
		{
			return row < info.row + info.rowspan && row + rowspan > info.row &&
				col < info.col + info.colspan && col + colspan > info.col;
		}
	};
	
	// Struct to specifiy a link between two volume renderers
	struct Link
	{
		VolumeRenderer* from = nullptr; VolumeRenderer* to = nullptr; LinkType type = LinkType::eNone;
	};

	// Struct to hold the required information for a region; combines regions with color maps
	struct RegionInfo
	{
		const Region* region = nullptr;
		const ColorMap1D* colorMap1D = nullptr;
		const ColorMap2D* colorMap2D = nullptr;
		const ColorMap1D* colorMap1DAlpha = nullptr;
	};

	// Struct to hold a collection of regions; combines regions into a priority list
	struct RegionInfoCollection
	{
		QString name = "Configuration";
		std::vector<RegionInfo> regions;
	};

	// Struct to hold all relevant information for a volume renderer
	struct VolumeRendererInfo
	{
		LayoutInfo layout; // The position inside the grid layout
		const HCNode* node = nullptr; // The corresponding dendrogram node
		std::shared_ptr<Ensemble> ensemble; // The corresponding sub-ensemble
		RegionInfoCollection* regions = nullptr; // The collection of regions used by the volume renderer
	};

	// Constructor :)
	VolumeRendererManager( std::shared_ptr<Ensemble> ensemble, Dendrogram& dendrogram, ParallelCoordinates& parallelCoordinates ) : QWidget(), _overlay( new Overlay( this ) ), _ensemble( ensemble ), _dendrogram( dendrogram ), _parallelCoordinates( parallelCoordinates )
	{
		this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );
		this->setStyleSheet( "background: transparent" );
		this->setMouseTracking( true );

		// Initialize overlay
		_overlay->setAttribute( Qt::WA_TransparentForMouseEvents, true );
		_overlay->setEnabled( false );
		_overlay->raise();

		// Intitialize connections
		QObject::connect( this, &VolumeRendererManager::numRowsChanged, this, &VolumeRendererManager::updateLayout );
		QObject::connect( this, &VolumeRendererManager::numColumnsChanged, this, &VolumeRendererManager::updateLayout );

		QObject::connect( &_dendrogram, &Dendrogram::hoveredNodeChanged, [=] ( const HCNode* hoveredNode )
		{
			_hoveredVolumeRenderer = nullptr;
			if( hoveredNode ) for( const auto& [volumeRenderer, info] : _volumeRenderers ) if( info.node == hoveredNode )
				_hoveredVolumeRenderer = volumeRenderer;
			this->update();
		} );
		QObject::connect( &_dendrogram, &Dendrogram::selectedNodeChanged, [=] ( const HCNode* node )
		{
			if( _selectedVolumeRenderer && _volumeRenderers[_selectedVolumeRenderer].node != node )
				this->setVolumeRendererNode( _selectedVolumeRenderer, node );
		} );
		QObject::connect( &_dendrogram, &Dendrogram::rootChanged, [=] ( const HCNode* node )
		{
			for( auto& [volumeRenderer, info] : _volumeRenderers ) info.node = nullptr;
			this->updateRootVolumeRenderers();
		} );
		QObject::connect( this, &VolumeRendererManager::selectedVolumeRendererChanged, [=] ( VolumeRenderer* volumeRenderer )
		{
			_dendrogram.setSelectedNode( volumeRenderer ? _volumeRenderers[volumeRenderer].node : nullptr );
		} );

		QObject::connect( &_settings, &VolumeRendererSettings::showHighlightedRegionChanged, [=] ( bool show )
		{
			if( show )
			{
				auto masks = std::unordered_map<const Ensemble*, std::shared_ptr<Volume<float>>>();
				for( auto& [volumeRenderer, info] : _volumeRenderers )
				{
					auto& mask = masks[info.ensemble.get()];
					if( !mask ) mask = _currentRegion->createMask( *info.ensemble );
					volumeRenderer->setHighlightedRegion( mask );
				}
			}
		} );

		this->setInteractionMode( InteractionMode::eViewing );
	}

	// Getter for the settings shared between all volume renderers
	VolumeRendererSettings& settings() noexcept
	{
		return _settings;
	}

	// Getter for a region collection
	const RegionInfoCollection& regionCollection( int32_t index ) const
	{
		return *_regionCollections[index];
	}
	
	// Getter for the info about a region
	const RegionInfo& regionInfo( int32_t index )
	{
		return _currentRegionCollection->regions[index];
	}

signals:
	// Signals for when the size of the grid layout changes
	void numRowsChanged( int32_t );
	void numColumnsChanged( int32_t );

	// Signal for when the interaction mode changes
	void interactionModeChanged( InteractionMode );

	// Signal for when the selected volume renderer changes
	void selectedVolumeRendererChanged( VolumeRenderer* );

	// Signal for when the ensembles that should be used throughout the application change (in parallel coordinates, color map editors etc.)
	void ensemblesChanged( std::shared_ptr<Ensemble>, std::shared_ptr<Ensemble> );

public slots:
	// Functions to add a row/column to the layout
	void addRow()
	{
		emit numRowsChanged( ++_numRows );
	}
	void addColumn()
	{
		emit numColumnsChanged( ++_numCols );
	}
	
	// Functions to remove a row/column from the layout, if possible
	void removeRow()
	{
		if( _numRows <= 1 ) return;

		// Find row that can be removed
		for( int32_t row = 0; row < _numRows; ++row )
		{
			// Check if volume renderer occupies row
			bool remove = true;
			for( const auto& [volumeRenderer, info] : _volumeRenderers )
			{
				if( info.layout.intersectsRow( row ) )
				{
					remove = false;
					break;
				}
			}

			// Remove row if possible
			if( remove )
			{
				for( auto& [volumeRenderer, info] : _volumeRenderers )
					if( info.layout.row > row ) --info.layout.row;

				--_numRows;
				emit numRowsChanged( _numRows );
				break;
			}
		}
	}
	void removeColumn()
	{
		if( _numCols <= 1 ) return;

		// Find row that can be removed
		for( int32_t col = 0; col < _numCols; ++col )
		{
			// Check if volume renderer occupies row
			bool remove = true;
			for( const auto& [volumeRenderer, info] : _volumeRenderers )
			{
				if( info.layout.intersectsColumn( col ) )
				{
					remove = false;
					break;
				}
			}

			// Remove row if possible
			if( remove )
			{
				for( auto& [volumeRenderer, info] : _volumeRenderers )
					if( info.layout.col > col ) --info.layout.col;

				--_numCols;
				emit numColumnsChanged( _numCols );
				break;
			}
		}
	}

	// Setter for the interaction mode
	void setInteractionMode( InteractionMode mode )
	{
		if( mode == _interactionMode ) return;
		emit interactionModeChanged( _interactionMode = mode );

		// Disable mouse events for volume renderers when not in viewing mode
		const auto transparentForMouseEvents = _interactionMode != InteractionMode::eViewing;
		for( auto& [volumeRenderer, info] : _volumeRenderers )
			volumeRenderer->setAttribute( Qt::WA_TransparentForMouseEvents, transparentForMouseEvents );

		this->update();
	}

	// Setter for the selected volume renderer
	void setSelectedVolumeRenderer( VolumeRenderer* volumeRenderer )
	{
		if( volumeRenderer != _selectedVolumeRenderer )
		{
			// Only root volume renderers can be selected
			if( volumeRenderer && !_rootVolumeRenderers.count( volumeRenderer ) ) return;

			// Selecting the already selected volume renderer will deselect it
			if( volumeRenderer == _selectedVolumeRenderer ) volumeRenderer = nullptr;

			emit selectedVolumeRendererChanged( _selectedVolumeRenderer = volumeRenderer );
			this->update();
		}
	}

	// Function to add/remove a regionCollection
	void addRegionCollection()
	{
		_regionCollections.emplace_back( new RegionInfoCollection );
		_currentRegionCollection = _regionCollections.back().get();
	}
	void removeRegionCollection( int32_t index )
	{
		if( _regionCollections.size() > 1 )
		{
			for( auto& [volumeRenderer, info] : _volumeRenderers ) if( info.regions == _regionCollections[index].get() )
				this->setRegionCollection( volumeRenderer, _regionCollections[!index].get() );
			_regionCollections.erase( _regionCollections.begin() + index );
		}
	}

	// Function to change the name of a region collection
	void setRegionCollectionName( int32_t index, QString name )
	{
		_regionCollections[index]->name = std::move( name );
		if( _interactionMode == InteractionMode::eEditing ) this->update();
	}

	// Function to update the current region collection
	void setCurrentRegionCollection( int32_t index )
	{
		_currentRegionCollection = _regionCollections[index].get();
	}

	// Functions to add/remove a region from the current region collection
	void addRegion( int32_t index, const Region* region )
	{
		this->addRegion( *_currentRegionCollection, index, RegionInfo { region } );
	}
	void removeRegion( int32_t index )
	{
		this->removeRegion( *_currentRegionCollection, index );
	}

	// Function to remove a region from all region collections
	void removeRegionForAll( const Region* region )
	{
		for( auto regionInfoCollection : _regionCollections )
			for( int32_t i = 0; i < regionInfoCollection->regions.size(); ++i ) if( regionInfoCollection->regions[i].region == region )
				this->removeRegion( *regionInfoCollection, i );
	}

	// Function to swap the order/priority of two regions in the current region collection
	void swapRegions( int32_t first, int32_t second )
	{
		std::swap( _currentRegionCollection->regions[first], _currentRegionCollection->regions[second] );
		for( auto& [volumeRenderer, info] : _volumeRenderers ) if( info.regions == _currentRegionCollection )
			volumeRenderer->swapRegions( first, second );
	}

	// Setter for the current region (used for region brushing)
	void setCurrentRegion( Region* region )
	{
		if( _currentRegion ) QObject::disconnect( _currentRegion, &Region::selectionChanged, this, &VolumeRendererManager::updateHighlightedRegions );
		_currentRegion = region;
		if( _currentRegion ) QObject::connect( _currentRegion, &Region::selectionChanged, this, &VolumeRendererManager::updateHighlightedRegions );
		this->updateHighlightedRegions();
	}

	// Setters for the color maps of the current region collection
	void setColorMap1D( int32_t regionIndex, const ColorMap1D* colorMap )
	{
		_currentRegionCollection->regions[regionIndex].colorMap1D = colorMap;
		this->updateRegions();
	}
	void setColorMap2D( int32_t regionIndex, const ColorMap2D* colorMap )
	{
		_currentRegionCollection->regions[regionIndex].colorMap2D = colorMap;
		this->updateRegions();
	}
	void setColorMap1DAlpha( int32_t regionIndex, const ColorMap1D* colorMap )
	{
		_currentRegionCollection->regions[regionIndex].colorMap1DAlpha = colorMap;
		this->updateRegions();
	}

	// Function to replace one color map with another
	void replaceColorMap1D( const ColorMap1D* from, const ColorMap1D* to )
	{
		for( auto regionCollection : _regionCollections )
		{
			for( int32_t i = 0; i < regionCollection->regions.size(); ++i )
			{
				auto& info = regionCollection->regions[i];

				if( info.colorMap1D == from ) info.colorMap1D = to;
				if( info.colorMap1DAlpha == from ) info.colorMap1DAlpha = to;
				for( auto& [volumeRenderer, info] : _volumeRenderers ) if( info.regions == regionCollection.get() )
					this->updateRegion( volumeRenderer, i );
			}
		}
	}
	void replaceColorMap2D( const ColorMap2D* from, const ColorMap2D* to )
	{
		for( auto regionCollection : _regionCollections )
		{
			for( int32_t i = 0; i < regionCollection->regions.size(); ++i )
			{
				auto& info = regionCollection->regions[i];

				if( info.colorMap2D == from ) info.colorMap2D = to;
				for( auto& [volumeRenderer, info] : _volumeRenderers ) if( info.regions == regionCollection.get() )
					this->updateRegion( volumeRenderer, i );
			}
		}
	}

	// Function to automatically create a hierarchy of volume renderers given a similarity threshold
	void performAutomaticLayout( float similarity )
	{
		// Delete all volume renderers and their links
		for( auto& [volumeRenderer, info] : _volumeRenderers ) volumeRenderer->deleteLater();
		_volumeRenderers.clear();
		_links.clear();

		// Iterate tree and add volume renderers
		const auto [volumeRenderer, maxrow, colspan] = this->performAutomaticLayout( similarity, _dendrogram.root(), 0, 0 );
		_numRows = maxrow + 1;
		_numCols = colspan;

		this->updateRootVolumeRenderers();
		this->setSelectedVolumeRenderer( volumeRenderer );

		_ensembleVolumeRenderer = volumeRenderer;
		emit volumeRenderer->requestEnsembleUsage();

		this->updateLayout();
	}

private slots:
	// Update the currently hovered grid cell
	void updateHoveredGridCell( QPoint point )
	{
		_hoveredGridCell = this->pointToGridCell( point );
		_hoveredVolumeRenderer = this->gridCellToVolumeRenderer( _hoveredGridCell );
		_hoveringCenter = this->checkHoveringCenter( _hoveredVolumeRenderer, point );

		_dendrogram.setHoveredNode( _hoveredVolumeRenderer ? _volumeRenderers[_hoveredVolumeRenderer].node : nullptr );
		this->update();
	}
	
	// Update the geometries of all volume renderers based on their layout information
	void updateLayout()
	{
		_overlay->setGeometry( this->rect() );

		for( auto& [volumeRenderer, info] : _volumeRenderers )
			volumeRenderer->setGeometry( this->layoutInfoToRect( info.layout ) );
		this->update();
	}

	// Add a link, check constraints; if link already exists, remove it
	void updateLink( VolumeRenderer* from, VolumeRenderer* to, LinkType type )
	{
		if( from == to ) return;

		bool addLink = true;

		// Remove conflicting links
		for( auto it = _links.begin(); it != _links.end(); )
		{
			// Check outgoing links
			if( it->from == from )
			{
				if( it->to == to )
				{
					if( it->type == type ) addLink = false;
					it = _links.erase( it );
				}
				else
				{
					if( it->type == type ) it = _links.erase( it );
					else ++it;
				}
			}
			else
			{
				if( ( it->from == to || it->to == from ) && it->type == LinkType::eSibling && type == LinkType::eSibling ) it = _links.erase( it );
				else if( it->to == to ) it = _links.erase( it );
				else ++it;
			}
		}

		if( addLink )
		{
			_links.push_back( Link { from, to, type } );

			// Check for outgoing links from 'to', remove them if they are part of a cycle
			for( auto it = _links.begin(); it != _links.end(); )
			{
				if( it->from == to )
				{
					// Check for cycle
					bool hasCycle = false;

					auto visited = std::unordered_set<VolumeRenderer*> { to };
					auto pending = std::stack<VolumeRenderer*>();
					pending.push( to );

					while( !hasCycle && pending.size() )
					{
						auto current = pending.top();
						pending.pop();

						for( const auto& [from, to, type] : _links ) if( from == current )
						{
							if( visited.count( to ) )
							{
								// Cycle detected
								hasCycle = true;
								break;
							}
							else pending.push( to );
						}
					}


					if( hasCycle ) it = _links.erase( it );
					else ++it;
				}
				else ++it;
			}
		}
		this->updateRootVolumeRenderers();
	}

	// Update the entire volume renderer hierarchy starting at the root volume renderers
	void updateRootVolumeRenderers()
	{
		// Add all volume renderers as root
		_rootVolumeRenderers.clear();
		for( const auto& [volumeRenderer, info] : _volumeRenderers )
			_rootVolumeRenderers.insert( volumeRenderer );

		// Remove volume renderers with an inbound edge
		for( const auto& [from, to, type] : _links ) _rootVolumeRenderers.erase( to );

		// Update volume renderer nodes
		for( auto volumeRenderer : _rootVolumeRenderers )
		{
			if( _volumeRenderers[volumeRenderer].node )
				this->setVolumeRendererNode( volumeRenderer, _volumeRenderers[volumeRenderer].node );
			else this->setVolumeRendererNode( volumeRenderer, _dendrogram.root() );
		}

		// Gather all nodes and signal dendrogram
		auto nodes = std::unordered_set<const HCNode*>();
		for( const auto& [volumeRenderer, info] : _volumeRenderers ) nodes.insert( info.node );
		_dendrogram.setHighlightedNodes( std::move( nodes ) );

		// Check if selected volume renderer is still root
		if( !_rootVolumeRenderers.count( _selectedVolumeRenderer ) )
			this->setSelectedVolumeRenderer( nullptr );
	}
	
	// Update the corresponding dendrogram node of a volume renderer
	void setVolumeRendererNode( VolumeRenderer* volumeRenderer, const HCNode* node )
	{
		if( node != _volumeRenderers[volumeRenderer].node )
		{
			// If the node changed, change the (sub-)ensemble of this volume renderer
			_volumeRenderers[volumeRenderer].node = node;

			// Create sub-ensemble
			if( node )
			{
				const auto ensemble = _volumeRenderers[volumeRenderer].ensemble = ( node == _dendrogram.root() ) ? _ensemble :
					std::make_shared<Ensemble>( _ensemble->createSubEnsemble( node->values() ) );
				if( volumeRenderer == _selectedVolumeRenderer )	_dendrogram.setSelectedNode( node );

				if( volumeRenderer == _ensembleVolumeRenderer )
				{
					std::shared_ptr<Ensemble> otherEnsemble;
					for( const auto& link : _links ) if( link.to == volumeRenderer && ( otherEnsemble = _volumeRenderers[link.from].ensemble ) ) break;
					emit ensemblesChanged( ensemble, otherEnsemble );
				}

				this->updateRegions( volumeRenderer );
				this->updateMasks( volumeRenderer );
			}
			else volumeRenderer->region( 0 )->setFirstVolume( nullptr, "" );


			// Gather all nodes and signal dendrogram
			auto nodes = std::unordered_set<const HCNode*>();
			for( const auto& [volumeRenderer, info] : _volumeRenderers ) nodes.insert( info.node );
			_dendrogram.setHighlightedNodes( std::move( nodes ) );
		}

		// Update linked volume renderers
		for( const auto& [from, to, type] : _links ) if( from == volumeRenderer )
		{
			if( node )
			{
				if( type == LinkType::eLeft ) this->setVolumeRendererNode( to, node->left() );
				else if( type == LinkType::eRight ) this->setVolumeRendererNode( to, node->right() );
				else if( type == LinkType::eSibling )
				{
					if( node->parent() )
					{
						if( node == node->parent()->left() ) this->setVolumeRendererNode( to, node->parent()->right() );
						else this->setVolumeRendererNode( to, node->parent()->left() );
					}
					else this->setVolumeRendererNode( to, nullptr );

				}
			}
			else this->setVolumeRendererNode( to, nullptr );
		}
		this->update();
	}

	// Setter for the region collection of a volume renderer
	void setRegionCollection( VolumeRenderer* volumeRenderer, RegionInfoCollection* regionCollection )
	{
		_volumeRenderers[volumeRenderer].regions = regionCollection;

		auto names = std::vector<QString>( regionCollection->regions.size() );
		for( int32_t i = 0; i < names.size(); ++i ) names[i] = regionCollection->regions[i].region->name();
		volumeRenderer->setRegionCount( names );

		this->updateRegions( volumeRenderer );
		this->updateMasks( volumeRenderer );
	}

	// Add or remove a region from a region collection
	void addRegion( RegionInfoCollection& regionInfoCollection, int32_t index, const RegionInfo& regionInfo )
	{
		regionInfoCollection.regions.insert( regionInfoCollection.regions.begin() + index, regionInfo );
		for( auto& [volumeRenderer, info] : _volumeRenderers ) if( info.regions == &regionInfoCollection )
			volumeRenderer->addRegion( index, regionInfo.region->name() );
		this->updateRegions();
		this->updateMasks();

		QObject::connect( regionInfo.region, &Region::selectionChanged, this, QOverload<>::of( &VolumeRendererManager::updateMasks ) );
		QObject::connect( regionInfo.region, &Region::nameChanged, [=] ( const QString& name )
		{
			for( auto& [volumeRenderer, info] : _volumeRenderers )
				for( int32_t i = 0; i < info.regions->regions.size(); ++i )
					if( info.regions->regions[i].region == regionInfo.region )
						volumeRenderer->setRegionName( i, name );
		} );
	}
	void removeRegion( RegionInfoCollection& regionInfoCollection, int32_t index )
	{
		QObject::disconnect( regionInfoCollection.regions[index].region, nullptr, this, nullptr );

		regionInfoCollection.regions.erase( regionInfoCollection.regions.begin() + index );
		for( auto& [volumeRenderer, info] : _volumeRenderers ) if( info.regions == &regionInfoCollection )
			volumeRenderer->removeRegion( index );

		if( regionInfoCollection.regions.empty() ) this->addRegion( regionInfoCollection, 0, _currentRegionCollection->regions.front() );
		else
		{
			this->updateRegions();
			this->updateMasks();
		}
	}

	// Functions to update volume renderer regions (update input volumes and color maps)
	void updateRegion( VolumeRenderer* volumeRenderer, int32_t index )
	{
		auto& volumeRendererInfo = _volumeRenderers[volumeRenderer];
		auto region = volumeRenderer->region( index );

		if( volumeRendererInfo.node )
		{
			const auto ensemble = volumeRendererInfo.ensemble;
			auto& regionInfo = volumeRendererInfo.regions->regions[index];

			const Ensemble* otherEnsemble = nullptr;
			for( const auto& link : _links ) if( link.to == volumeRenderer && ( otherEnsemble = _volumeRenderers[link.from].ensemble.get() ) ) break;

			region->setColorMap( regionInfo.colorMap1D );
			region->setColorMap2D( regionInfo.colorMap2D );
			region->setColorMapAlpha( regionInfo.colorMap1DAlpha );

			const auto getVolume = [&] ( const Ensemble::VolumeID& id )
			{
				const Volume<float>* volume = nullptr;
				if( id.difference && otherEnsemble ) volume = &ensemble->differenceVolume( id, *otherEnsemble );
				else volume = &ensemble->volume( id );

				const auto name = ensemble->fieldCount() > 1 ? ensemble->field( id.field ).name() : "";
				return std::make_pair( volume, name );
			};

			if( regionInfo.colorMap1D )
			{
				const auto [volume, name] = getVolume( regionInfo.colorMap1D->volumeID() );
				region->setFirstVolume( volume, name );
			}
			if( regionInfo.colorMap2D )
			{
				const auto [volume, name] = getVolume( regionInfo.colorMap2D->volumeIDs().second );
				region->setSecondVolume( volume, name );
			}
			if( regionInfo.colorMap1DAlpha )
			{
				const auto [volume, name] = getVolume( regionInfo.colorMap1DAlpha->volumeID() );
				region->setAlphaVolume( volume, name );
			}
		}
		else region->setFirstVolume( nullptr, "" );
	}
	void updateRegions( VolumeRenderer* volumeRenderer )
	{
		for( int32_t i = 0; i < _volumeRenderers[volumeRenderer].regions->regions.size(); ++i )
			this->updateRegion( volumeRenderer, i );
	}
	void updateRegions()
	{
		for( auto& [volumeRenderer, info] : _volumeRenderers )
			this->updateRegions( volumeRenderer );
	}

	// Functions to update volume renderer masks
	void updateMask( VolumeRenderer* volumeRenderer, int32_t index )
	{
		auto& volumeRendererInfo = _volumeRenderers[volumeRenderer];
		if( volumeRendererInfo.node )
		{
			const auto& ensemble = volumeRendererInfo.ensemble;
			auto& regionInfo = volumeRendererInfo.regions->regions[index];
			auto region = volumeRenderer->region( index );

			if( regionInfo.region ) region->setMask( regionInfo.region->createMask( *ensemble ) );
			else region->setMask( nullptr );
		}
	}
	void updateMasks( VolumeRenderer* volumeRenderer )
	{
		for( int32_t i = 0; i < _volumeRenderers[volumeRenderer].regions->regions.size(); ++i )
			this->updateMask( volumeRenderer, i );
	}
	void updateMasks()
	{
		for( auto& [volumeRenderer, info] : _volumeRenderers )
			this->updateMasks( volumeRenderer );
	}

	// Function to create/delete a volume renderer
	VolumeRenderer* createVolumeRenderer()
	{
		auto volumeRenderer = new VolumeRenderer( _settings, this );

		auto names = std::vector<QString>( _currentRegionCollection->regions.size() );
		for( int32_t i = 0; i < names.size(); ++i ) names[i] = _currentRegionCollection->regions[i].region->name();
		volumeRenderer->setRegionCount( names );

		_volumeRenderers[volumeRenderer].layout = LayoutInfo( 0, 0, 1, 1 );
		_volumeRenderers[volumeRenderer].regions = _currentRegionCollection;

		QObject::connect( volumeRenderer, &VolumeRenderer::highlightedRegionChanged, [=] ( std::shared_ptr<Volume<float>> volume )
		{
			if( _currentRegion ) _currentRegion->setConstantMask( volume );
		} );
		QObject::connect( volumeRenderer, &VolumeRenderer::requestEnsembleUsage, [=]
		{
			_ensembleVolumeRenderer = volumeRenderer;
			std::shared_ptr<Ensemble> otherEnsemble;
			for( const auto& link : _links ) if( link.to == volumeRenderer && ( otherEnsemble = _volumeRenderers[link.from].ensemble ) ) break;
			emit ensemblesChanged( _volumeRenderers[volumeRenderer].ensemble, otherEnsemble );
		} );

		return volumeRenderer;
	}
	void deleteVolumeRenderer( VolumeRenderer* volumeRenderer )
	{
		// Remove all links
		for( auto it = _links.begin(); it != _links.end(); )
		{
			if( it->from == volumeRenderer || it->to == volumeRenderer )
				it = _links.erase( it );
			else ++it;
		}

		// Remove from node list
		_volumeRenderers.erase( volumeRenderer );
		volumeRenderer->deleteLater();

		// Links may have changed, therefore update root volume renderers (this will also deselect this volume renderer if necessary)
		this->updateRootVolumeRenderers();
	}

	// Function to update the currently highlighted region (for region brushing)
	void updateHighlightedRegions()
	{
		if( _settings.showHighlightedRegion() ) for( auto& [volumeRenderer, info] : _volumeRenderers )
			volumeRenderer->setHighlightedRegion( _currentRegion->createMask( *info.ensemble ) );
	}

private:
	// Handle resize events
	void resizeEvent( QResizeEvent* event ) override
	{
		QWidget::resizeEvent( event );

		// On first resize, create the first volume renderer
		if( _volumeRenderers.empty() )
		{
			auto volumeRenderer = this->createVolumeRenderer();
			volumeRenderer->stackUnder( _overlay );
			volumeRenderer->show();

			QObject::connect( volumeRenderer, &VolumeRenderer::initialized, [=]
			{
				this->updateRootVolumeRenderers();
				this->setSelectedVolumeRenderer( volumeRenderer );
				_ensembleVolumeRenderer = volumeRenderer;
			} );
		}

		this->updateLayout();
	}
	
	// Draw the overlay and volume renderer graph
	void paintEvent( QPaintEvent* event ) override
	{
		_overlay->image().fill( Qt::transparent );
		auto painter = QPainter( &_overlay->image() );

		const auto selectionRect = QRect( _selectionBegin, _selectionBegin == QPoint( -1, -1 ) ? QPoint( -1, -1 ) : _cursor );

		// Draw empty cell
		painter.setPen( QColor( 0, 0, 0 ) );
		for( int32_t row = 0; row < _numRows; ++row )
		{
			for( int32_t col = 0; col < _numCols; ++col )
			{
				const auto gridCell = GridCell( row, col );
				const auto volumeRenderer = this->gridCellToVolumeRenderer( gridCell );
				if( volumeRenderer ) continue;

				const auto info = LayoutInfo( row, col, 1, 1 );
				const auto rect = this->layoutInfoToRect( info );

				auto brush = QColor( 0, 0, 0, 0 );
				if( rect.intersects( selectionRect ) || ( !_linkBegin && gridCell == _hoveredGridCell ) ) brush = QColor( 200, 222, 249, 50 );

				painter.setBrush( brush );
				painter.drawRect( rect );
			}
		}

		const auto drawLink = [&] ( QPoint begin, QPoint end, LinkType type, bool showDirection )
		{
			auto color = QColor( 26, 26, 26 );
			if( type == LinkType::eLeft ) color = QColor( 115, 232, 26 );
			else if( type == LinkType::eRight ) color = QColor( 232, 26, 115 );

			auto penStyle = Qt::PenStyle::SolidLine;
			if( type == LinkType::eSibling ) penStyle = Qt::PenStyle::DotLine;

			painter.setPen( QPen( color, 5.0, penStyle, Qt::PenCapStyle::RoundCap ) );
			painter.drawLine( begin, end );

			if( showDirection )
			{
				auto line = QLineF( begin, end );
				line.setLength( 10 );

				painter.setPen( Qt::transparent );
				painter.setBrush( color );
				painter.drawEllipse( end + ( line.p1() - line.p2() ), 7, 7 );
			}
		};

		// Draw links
		if( _interactionMode == InteractionMode::eEditing )
		{
			painter.setRenderHint( QPainter::Antialiasing, true );
			for( const auto& [from, to, type] : _links ) if( from->isVisible() && to->isVisible() )
			{
				const auto begin = this->layoutInfoToRect( _volumeRenderers[from].layout ).center();
				const auto end = this->layoutInfoToRect( _volumeRenderers[to].layout ).center();
				drawLink( begin, end, type, true );
			}
			painter.setRenderHint( QPainter::Antialiasing, false );
		}

		// Draw active link arrow
		if( _linkBegin )
		{
			const auto begin = this->layoutInfoToRect( _volumeRenderers[_linkBegin].layout ).center();

			painter.setRenderHint( QPainter::Antialiasing, true );
			drawLink( begin, _cursor, _linkType, false );
			painter.setRenderHint( QPainter::Antialiasing, false );
		}

		// Draw cells with volume renderers
		for( const auto& [volumeRenderer, info] : _volumeRenderers )
		{
			// Draw border
			const auto rect = this->layoutInfoToRect( info.layout );

			auto brush = QColor( 0, 0, 0, 0 );
			if( _interactionMode == InteractionMode::eEditing )
			{
				const auto hovered = ( !_linkBegin && volumeRenderer == _hoveredVolumeRenderer && !_hoveringCenter );
				if( rect.intersects( selectionRect ) ) brush = QColor( 249, 200, 222, 100 );
				else if( volumeRenderer == _selectedVolumeRenderer ) brush = QColor( 26, 115, 232, hovered ? 90 : 100 );
				else if( hovered ) brush = QColor( 200, 222, 249, 80 );

				auto font = this->font();
				font.setPointSize( 14 );
				painter.setFont( font );
				painter.setPen( Qt::black );
				painter.drawText( rect.marginsRemoved( QMargins( 5, 5, 5, 5 ) ), Qt::AlignLeft | Qt::AlignTop, info.regions->name );
			}

			painter.setBrush( brush );
			painter.setPen( QColor( 0, 0, 0 ) );
			painter.drawRect( rect );

			// Draw center circle
			if( _interactionMode == InteractionMode::eEditing )
			{
				painter.setRenderHint( QPainter::Antialiasing, true );

				auto brush = QColor();
				auto pen = QColor();

				if( ( volumeRenderer == _linkBegin ) || ( volumeRenderer == _hoveredVolumeRenderer && _hoveringCenter ) )
				{
					painter.setBrush( QColor( 200, 222, 249, 255 ) );
					painter.setPen( QColor( 26, 115, 232, 255 ) );
				}
				else
				{
					painter.setBrush( QColor( 200, 200, 200, 255 ) );
					painter.setPen( QColor( 0, 0, 0, 255 ) );
				}

				if( _rootVolumeRenderers.count( volumeRenderer ) )
					painter.drawEllipse( rect.center(), 15, 15 );
				painter.drawEllipse( rect.center(), 10, 10 );

				painter.setRenderHint( QPainter::Antialiasing, false );
			}
		}

		// Draw selection rectangle
		painter.setRenderHint( QPainter::Antialiasing, true );
		painter.setBrush( QColor( 200, 222, 249, 100 ) );
		painter.setPen( QColor( 26, 115, 232, 100 ) );
		painter.drawRect( selectionRect );
	}

	// Handle mouse move events
	void mouseMoveEvent( QMouseEvent* event ) override
	{
		if( _interactionMode == InteractionMode::eEditing )
			this->updateHoveredGridCell( _cursor = event->pos() );
	}
	
	// Reset the currently hovered grid cell/center when the mouse leaves
	void leaveEvent( QEvent* event ) override
	{
		_cursor = QPoint( -1, -1 );
		_hoveredGridCell = GridCell( -1, -1 );
		_hoveredVolumeRenderer = nullptr;
		_hoveringCenter = false;
		_dendrogram.setHoveredNode( nullptr );
		this->update();
	}

	// Handle mouse press events
	void mousePressEvent( QMouseEvent* event ) override
	{
		if( _interactionMode == InteractionMode::eViewing ) return;

		if( _hoveredVolumeRenderer )
		{
			if( _hoveringCenter )
			{
				// When clicking the center, initiate linking mode
				_linkBegin = _hoveredVolumeRenderer;
				if( event->button() == Qt::LeftButton ) _linkType = LinkType::eLeft;
				else if( event->button() == Qt::RightButton ) _linkType = LinkType::eRight;
				else if( event->button() == Qt::MiddleButton ) _linkType = LinkType::eSibling;
			}
			else
			{
				if( event->button() == Qt::LeftButton ) this->setSelectedVolumeRenderer( _hoveredVolumeRenderer );
				else if( event->button() == Qt::RightButton || event->button() == Qt::MiddleButton )
				{
					// Hide volume renderer and remove it from layout
					_hoveredVolumeRenderer->setVisible( false );
					_volumeRenderers.erase( _hoveredVolumeRenderer );

					if( event->button() == Qt::MiddleButton )
					{
						// Remember removed volume renderers (to maybe add them back later)
						_removedVolumeRenderers.push( _hoveredVolumeRenderer );
						this->updateRootVolumeRenderers();
					}
					else this->deleteVolumeRenderer( _hoveredVolumeRenderer );

					this->updateHoveredGridCell( _cursor = event->pos() );
				}
			}
		}
		else
		{
			if( event->button() == Qt::LeftButton )
			{
				// Begin selection of region for new volume renderer
				_selectionBegin = event->pos();
			}
		}
	}

	// Handle mouse release events
	void mouseReleaseEvent( QMouseEvent* event ) override
	{
		if( _interactionMode == InteractionMode::eEditing )
		{
			if( _linkType != LinkType::eNone )
			{
				// Update link between volume renderers
				if( _hoveredVolumeRenderer && _hoveringCenter )
				{
					if( _linkBegin == _hoveredVolumeRenderer && _rootVolumeRenderers.count( _linkBegin ) )
						this->setSelectedVolumeRenderer( _hoveredVolumeRenderer );
					else this->updateLink( _linkBegin, _hoveredVolumeRenderer, _linkType );
				}

				_linkBegin = nullptr;
				_linkType = LinkType::eNone;
				this->updateHoveredGridCell( _cursor = event->pos() );
			}
			if( event->button() == Qt::LeftButton )
			{
				if( _selectionBegin != QPoint( -1, -1 ) )
				{
					// Determine layout information
					const auto topleft = this->pointToGridCell( _selectionBegin );
					const auto bottomright = this->pointToGridCell( _cursor );

					const auto beginrow = std::min( topleft.row, bottomright.row );
					const auto endrow = std::max( topleft.row, bottomright.row );
					const auto begincol = std::min( topleft.col, bottomright.col );
					const auto endcol = std::max( topleft.col, bottomright.col );

					const auto layoutInfo = LayoutInfo( beginrow, begincol, endrow - beginrow + 1, endcol - begincol + 1 );

					// Make sure no existing volume renderer intersects
					bool intersects = false;
					for( const auto& [volumeRenderer, info] : _volumeRenderers )
					{
						if( layoutInfo.intersectsLayoutInfo( info.layout ) )
						{
							intersects = true;
							break;
						}
					}

					// Add a new volume renderer at the specified location
					if( !intersects )
					{
						VolumeRenderer* volumeRenderer = nullptr;
						if( _removedVolumeRenderers.empty() ) volumeRenderer = this->createVolumeRenderer();
						else
						{
							volumeRenderer = _removedVolumeRenderers.top();
							_removedVolumeRenderers.pop();
						}

						_volumeRenderers[volumeRenderer].layout = layoutInfo;

						volumeRenderer->setAttribute( Qt::WA_TransparentForMouseEvents, true );
						volumeRenderer->stackUnder( _overlay );
						volumeRenderer->show();

						this->updateRootVolumeRenderers();
						this->updateLayout();
					}

					_selectionBegin = QPoint( -1, -1 );
					this->updateHoveredGridCell( _cursor = event->pos() );
				}
			}
		}
	}

	// Handle mouse wheel events
	void wheelEvent( QWheelEvent* event ) override
	{
		if( _interactionMode == InteractionMode::eViewing || !_hoveredVolumeRenderer ) return;

		// Iterate through the region collection for the hovered volume renderer
		int32_t index = 0;
		for( ; index < _regionCollections.size(); ++index ) if( _regionCollections[index].get() == _volumeRenderers[_hoveredVolumeRenderer].regions ) break;
		const auto next = std::clamp( index + ( event->delta() > 0 ? 1 : -1 ), 0, static_cast<int32_t>( _regionCollections.size() - 1 ) );

		if( next != index )
		{
			this->setRegionCollection( _hoveredVolumeRenderer, _regionCollections[next].get() );
			this->update();
		}
	}

	// Helper function to convert a layout info to a rectangle of screen coordinates
	QRect layoutInfoToRect( const LayoutInfo& info )
	{
		const auto rowstep = ( this->height() - 1 ) / _numRows;
		const auto colstep = ( this->width() - 1 ) / _numCols;

		const auto x = info.col * colstep;
		const auto y = info.row * rowstep;

		const auto w = info.colspan * colstep;
		const auto h = info.rowspan * rowstep;

		return QRect( x, y, w, h );
	}

	// Helper function to determine the grid cell at a specified position
	GridCell pointToGridCell( QPoint point )
	{
		const auto rowstep = ( this->height() - 1 ) / _numRows;
		const auto colstep = ( this->width() - 1 ) / _numCols;

		const auto row = point.y() / rowstep;
		const auto col = point.x() / colstep;

		return GridCell( row, col );
	}

	// Helper function to find the volume renderer at the specified grid cell
	VolumeRenderer* gridCellToVolumeRenderer( GridCell cell )
	{
		for( auto& [volumeRenderer, info] : _volumeRenderers )
			if( info.layout.intersectsCell( cell ) ) return volumeRenderer;
		return nullptr;
	}

	// Helper function to check whether a point is at the center of a volume renderer
	bool checkHoveringCenter( VolumeRenderer* volumeRenderer, QPoint point )
	{
		if( !volumeRenderer ) return false;

		const auto rect = this->layoutInfoToRect( _volumeRenderers[volumeRenderer].layout );
		const auto distanceToCenter = QLineF( point, rect.center() ).length();
		return distanceToCenter < 10.0;
	}
	
	// Helper function to recursively perform an automatic layout of volume renderers using the similarity threshold
	std::tuple<VolumeRenderer*, int32_t, int32_t> performAutomaticLayout( float similarity, const HCNode* node, int32_t row, int32_t col )
	{
		auto volumeRenderer = this->createVolumeRenderer();
		volumeRenderer->setAttribute( Qt::WA_TransparentForMouseEvents, _interactionMode == InteractionMode::eEditing );
		volumeRenderer->stackUnder( _overlay );
		volumeRenderer->show();

		int32_t maximumRow = row, leftcols = 0, rightcols = 0;
		if( node->left() && node->left()->similarity() <= similarity )
		{
			const auto [renderer, maxrow, colspan] = this->performAutomaticLayout( similarity, node->left(), row + 1, col );
			_links.push_back( Link { volumeRenderer, renderer, LinkType::eLeft } );
			maximumRow = std::max( maximumRow, maxrow );
			leftcols = colspan;
		}

		if( node->right() && node->right()->similarity() <= similarity )
		{
			const auto [renderer, maxrow, colspan] = this->performAutomaticLayout( similarity, node->right(), row + 1, col + leftcols );
			_links.push_back( Link { volumeRenderer, renderer, LinkType::eRight } );
			maximumRow = std::max( maximumRow, maxrow );
			rightcols = colspan;
		}

		const auto colspan = std::max( 1, leftcols + rightcols );
		_volumeRenderers[volumeRenderer].layout = LayoutInfo( row, col, 1, colspan );
		return std::make_tuple( volumeRenderer, maximumRow, colspan );
	}

	Overlay* _overlay = nullptr;
	int32_t _numRows = 1, _numCols = 1;
	InteractionMode _interactionMode = InteractionMode::eViewing;

	QPoint _cursor = QPoint( -1, -1 );
	GridCell _hoveredGridCell = GridCell( -1, -1 );
	VolumeRenderer* _hoveredVolumeRenderer = nullptr;
	bool _hoveringCenter = false;

	std::unordered_map<VolumeRenderer*, VolumeRendererInfo> _volumeRenderers;
	std::vector<std::shared_ptr<RegionInfoCollection>> _regionCollections;
	RegionInfoCollection* _currentRegionCollection = nullptr;

	std::stack<VolumeRenderer*> _removedVolumeRenderers;
	QPoint _selectionBegin = QPoint( -1, -1 );

	std::vector<Link> _links;
	std::unordered_set<VolumeRenderer*> _rootVolumeRenderers;
	VolumeRenderer* _linkBegin = nullptr;
	LinkType _linkType = LinkType::eNone;

	Dendrogram& _dendrogram;
	VolumeRenderer* _selectedVolumeRenderer = nullptr;
	VolumeRenderer* _ensembleVolumeRenderer = nullptr;

	std::shared_ptr<Ensemble> _ensemble;
	VolumeRendererSettings _settings;

	ParallelCoordinates& _parallelCoordinates;
	Region* _currentRegion = nullptr;

};