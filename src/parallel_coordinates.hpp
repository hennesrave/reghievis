#pragma once
#include <qopenglwidget.h>
#include <qopenglframebufferobject.h>
#include <qopenglfunctions_4_5_core.h>
#include <qopenglshaderprogram.h>

#include <qpainter.h>
#include <qscrollarea.h>
#include <qscrollbar.h>
#include <qwidget.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <renderdoc_app.h>
#include <set>

#include "color_map.hpp"
#include "region.hpp"

// Class for a single parallel coordinates axis
class ParallelCoordinatesAxis : public QWidget
{
	Q_OBJECT
public:
	// Costructor :)
	ParallelCoordinatesAxis( std::vector<vec2d>& intervals, const Volume<float>& volume ) : QWidget(), _volume( &volume )
	{
		this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );
		this->setObjectName( "parallel_coordinates_axis" );

		const auto range = vec2d( volume.domain() );
		auto onePercentStep = ( range.y - range.x ) / 100.0;
		auto singleStep = 1.0;
		auto precision = 1;
		while( onePercentStep && onePercentStep < 1.0 ) onePercentStep *= 10.0, singleStep /= 10.0, ++precision;

		// Initialize layout
		auto layout = new QVBoxLayout( this );
		layout->setContentsMargins( 0, 10, 0, 10 );
		layout->setSpacing( 0 );

		// First row
		layout->addWidget( _label = new QLabel( volume.name() ) );

		// Second row
		auto row = new QHBoxLayout();
		row->setContentsMargins( 0, 0, 0, 0 );
		row->setSpacing( 0 );
		row->addWidget( new QWidget(), 1 );
		row->addWidget( _buttonUp = new QPushButton() );
		row->addWidget( _buttonInvert = new QPushButton() );
		row->addWidget( _buttonDown = new QPushButton() );
		row->addWidget( new QWidget(), 1 );
		layout->addLayout( row );

		// Third row
		layout->addWidget( _upper = new QDoubleSpinBox(), 0, Qt::AlignCenter );

		// Fourth row
		layout->addWidget( _axisBar = new ParallelCoordinatesAxisBar( ParallelCoordinatesAxisBar::Direction::eVertical, intervals, range, precision ), 1 );

		// Fifth row
		layout->addWidget( _lower = new QDoubleSpinBox(), 0, Qt::AlignCenter );

		// Initialize widgets
		_label->setAlignment( Qt::AlignBottom | Qt::AlignHCenter );

		_lower->setButtonSymbols( QSpinBox::NoButtons );
		_lower->setSingleStep( singleStep );
		_lower->setDecimals( precision );
		_lower->setRange( range.x, range.y - singleStep );
		_lower->setValue( range.x );

		_upper->setButtonSymbols( QSpinBox::NoButtons );
		_upper->setSingleStep( singleStep );
		_upper->setDecimals( precision );
		_upper->setRange( range.x + singleStep, range.y );
		_upper->setValue( range.y );

		_buttonUp->setObjectName( "large_icon" );
		_buttonUp->setFixedSize( 20, 20 );
		_buttonUp->setStyleSheet( "image: url(:/west.png)" );

		_buttonInvert->setObjectName( "large_icon" );
		_buttonInvert->setFixedSize( 20, 20 );
		_buttonInvert->setStyleSheet( "image: url(:/invert.png)" );

		_buttonDown->setObjectName( "large_icon" );
		_buttonDown->setFixedSize( 20, 20 );
		_buttonDown->setStyleSheet( "image: url(:/east.png)" );

		// Initialize event handling
		QObject::connect( _lower, QOverload<double>::of( &QDoubleSpinBox::valueChanged ), [=] ( double value )
		{
			if( _upper->value() - value < singleStep ) _upper->setValue( value + singleStep );
			_axisBar->setCurrentRange( vec2d( _lower->value(), _upper->value() ) );
		} );
		QObject::connect( _upper, QOverload<double>::of( &QDoubleSpinBox::valueChanged ), [=] ( double value )
		{
			if( value - _lower->value() < singleStep ) _lower->setValue( value - singleStep );
			_axisBar->setCurrentRange( vec2d( _lower->value(), _upper->value() ) );
		} );
		QObject::connect( _buttonInvert, &QPushButton::clicked, _axisBar, &ParallelCoordinatesAxisBar::invertIntervals );
		QObject::connect( _buttonUp, &QPushButton::clicked, this, &ParallelCoordinatesAxis::requestMoveUp );
		QObject::connect( _buttonDown, &QPushButton::clicked, this, &ParallelCoordinatesAxis::requestMoveDown );
		QObject::connect( _axisBar, &ParallelCoordinatesAxisBar::currentRangeChanged, [=] ( vec2d range )
		{
			_hasMaximumRange = range == this->maximumRange();
			_lower->setValue( range.x );
			_upper->setValue( range.y );
			emit currentRangeChanged( range );
		} );
		QObject::connect( _axisBar, &ParallelCoordinatesAxisBar::intervalsChanged, this, &ParallelCoordinatesAxis::intervalsChanged );
	}

	// Setter and getter for the title of the axis
	void setTitle( const QString& title )
	{
		_label->setText( title );
	}
	const QString& title() const
	{
		return _label->text();
	}

	// Setter and getter for whether the axis should be movable
	void setMovable( bool movable )
	{
		_buttonUp->setVisible( movable );
		_buttonDown->setVisible( movable );
	}
	bool isMovable() const
	{
		return _buttonUp->isVisible();
	}

	// Setter for the highlighted value
	void setHighlightedValue( double value )
	{
		_axisBar->setHighlightedValue( value );
	}

	// Setter and getter for the intervals
	void setIntervals( std::vector<vec2d>& intervals )
	{
		_axisBar->setIntervals( intervals );
	}
	const std::vector<vec2d>& intervals() const noexcept
	{
		return _axisBar->intervals();
	}

	// Setter and getter for the input volume
	void setVolume( const Volume<float>& volume )
	{
		_volume = &volume;

		// Expand range
		const auto currentRange = vec2d( _lower->minimum(), _lower->maximum() );
		const auto volumeRange = vec2d( volume.domain() );
		const auto range = vec2d( std::min( currentRange.x, volumeRange.x ), std::max( currentRange.y, volumeRange.y ) );

		// Calculate step size and precision
		auto onePercentStep = ( range.y - range.x ) / 100.0;
		auto singleStep = 1.0;
		auto precision = 1;
		while( onePercentStep < 1.0 ) onePercentStep *= 10.0, singleStep /= 10.0, ++precision;

		this->blockSignals( true );
		// Update maximum (!) range
		_axisBar->setPrecision( precision );
		_axisBar->expandMaximumRange( range, false );

		_lower->setSingleStep( singleStep );
		_lower->setDecimals( precision );
		_lower->setRange( range.x, range.y - singleStep );

		_upper->setSingleStep( singleStep );
		_upper->setDecimals( precision );
		_upper->setRange( range.x + singleStep, range.y );

		// Adjust current range
		_axisBar->setCurrentRange( range );
		_hasMaximumRange = true;
		this->blockSignals( false );
	}
	const Volume<float>& volume() const noexcept
	{
		return *_volume;
	}

	// Getters for maximum and current range
	vec2d maximumRange() const
	{
		return _axisBar->maximumRange();
	}
	vec2d currentRange() const
	{
		return _hasMaximumRange ? this->maximumRange() : _axisBar->currentRange();
	}

	// Getter for mapping the position of the axis end points to the coordinates of another widget
	std::pair<QPoint, QPoint> mapAxesPoints( QWidget* widget ) const
	{
		return _axisBar->mapAxesPoints( widget );
	}

signals:
	// Signals when the current range changes
	void currentRangeChanged( vec2d );

	// Signals when the selected intervals change
	void intervalsChanged();

	// Requests a change in position for this axis
	void requestMoveUp();
	void requestMoveDown();

public slots:
	// Enables/disables real time brushing
	void setRealtimeEnabled( bool enabled )
	{
		_axisBar->setRealtimeEnabled( enabled );
	}

private:
	const Volume<float>* _volume = nullptr;
	bool _hasMaximumRange = true;

	QLabel* _label = nullptr;
	QDoubleSpinBox* _lower = nullptr;
	QDoubleSpinBox* _upper = nullptr;

	QPushButton* _buttonUp = nullptr;
	QPushButton* _buttonDown = nullptr;
	QPushButton* _buttonInvert = nullptr;

	ParallelCoordinatesAxisBar* _axisBar = nullptr;
};

// Class for the parallel coordinates visualization
class ParallelCoordinates : public QOpenGLWidget, public QOpenGLFunctions_4_5_Core
{
	Q_OBJECT
public:
	// Constructor :)
	ParallelCoordinates() : QOpenGLWidget()
	{
		this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );

		// Initlaize and layout widgets
		auto layout = new QHBoxLayout( this );
		layout->setContentsMargins( 10, 0, 10, 0 );
		layout->setSpacing( 50 );

		_scrollAreaWidget = new QWidget();
		_scrollAreaWidget->setObjectName( "parallel_coordinates_scroll_area_widget" );
		_scrollAreaWidget->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::MinimumExpanding );

		_axesLayout = new QHBoxLayout( _scrollAreaWidget );
		_axesLayout->setContentsMargins( 0, 0, 0, 0 );
		_axesLayout->setAlignment( Qt::AlignTop );
		_axesLayout->setSpacing( 10 );

		_scrollArea = new QScrollArea();
		_scrollArea->setObjectName( "parallel_coordinates_scroll_area" );
		_scrollArea->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
		_scrollArea->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
		_scrollArea->verticalScrollBar()->setEnabled( false );
		_scrollArea->setFrameShape( QFrame::NoFrame );
		_scrollArea->setWidgetResizable( true );
		_scrollArea->setWidget( _scrollAreaWidget );
		_scrollArea->installEventFilter( this );

		_histogramLayout = new QHBoxLayout();
		_histogramLayout->setContentsMargins( 0, 0, 0, 0 );
		_histogramLayout->setSpacing( 5 );

		layout->addWidget( _scrollArea, 1 );
		_axesLayout->addLayout( _histogramLayout );

		// Initialize connections
		QObject::connect( _scrollArea->horizontalScrollBar(), &QScrollBar::valueChanged, [=] { this->update(); } );
	}

	// Setter for the current region
	void setRegion( Region* region )
	{
		const auto enableAllAxes = !_region;

		// Disconnect old region
		if( _region ) QObject::disconnect( _region, nullptr, this, nullptr );

		// Connect new region
		_region = region;
		QObject::connect( _region, &Region::selectionChanged, this, &ParallelCoordinates::onSelectionChanged );

		// Update axes
		for( auto [type, axis] : _axes )
		{
			if( enableAllAxes ) _region->enabledAxes().insert( type );
			axis->setIntervals( _region->intervals( type ) );

			const auto enabled = _region->enabledAxes().count( type );
			axis->setVisible( enabled );
		}

		if( _initialized )
		{
			_updateWhenScrollAreaRepaints = true;

			// Updates the visible voxels since the current ranges of axes might have changes
			this->updateVisibilityBuffer();
		}
	}

	// Setter and getter for the current ensemble
	void setEnsemble( const Ensemble& ensemble )
	{
		if( _ensemble == &ensemble ) return;

		// Block signals
		for( const auto [key, axis] : _axes ) axis->blockSignals( true );

		// For the first ensemble, peform first-time setup of all axes
		if( !_ensemble )
		{
			const auto& available = ensemble.availableVolumes();
			for( const auto& id : available )
			{
				const auto volumeType = id.type;
				const auto& volume = ensemble.volume( id );

				auto axis = _axes[id] = new ParallelCoordinatesAxis( _region->intervals( id ), volume );
				_region->enabledAxes().insert( id );

				if( volumeType >= Ensemble::Derived::eHist1 && volumeType <= Ensemble::Derived::eHist5 )
				{
					const auto highlightedValues = std::vector<double> {
						// 0.0668, 0.0918, 0.1499, 0.3829, 0.1499, 0.0918, 0.0668
						0.2, 0.2, 0.2, 0.2, 0.2
					};

					axis->setTitle( volume.name().split( ' ' ).back() );
					axis->setMovable( false );
					axis->setHighlightedValue( highlightedValues[static_cast<int32_t>( volumeType ) - static_cast<int32_t>( Ensemble::Derived::eHist1 )] );
					_histogramLayout->addWidget( axis );
				}
				else _axesLayout->insertWidget( _axesLayout->count() - 1, axis );

				// Setup connections
				QObject::connect( axis, &ParallelCoordinatesAxis::currentRangeChanged, [=]
				{
					this->updateVisibilityBuffer();
				} );
				QObject::connect( axis, &ParallelCoordinatesAxis::intervalsChanged, [=] { this->updateSelectionBuffer(); } );

				QObject::connect( axis, &ParallelCoordinatesAxis::requestMoveUp, [=]
				{
					auto index = _axesLayout->indexOf( axis );
					if( index > 0 )
					{
						_axesLayout->removeWidget( axis );
						_axesLayout->insertWidget( index - 1, axis );
						this->update();
					}
				} );
				QObject::connect( axis, &ParallelCoordinatesAxis::requestMoveDown, [=]
				{
					auto index = _axesLayout->indexOf( axis );
					if( index < _axes.size() - 1 )
					{
						_axesLayout->removeWidget( axis );
						_axesLayout->insertWidget( index + 1, axis );
						this->update();
					}
				} );
			}
		}

		// Update axis volumes
		_ensemble = &ensemble;
		for( auto [volumeID, axis] : _axes ) if( _region->enabledAxes().count( volumeID ) )
			axis->setVolume( _ensemble->volume( volumeID ) );

		// Unblock signals
		for( const auto [key, axis] : _axes ) axis->blockSignals( false );

		// Update buffer of volume data
		if( _initialized ) this->updateVolumesBuffer( true, _region && !_region->constantMask() );

		emit ensembleChanged( _ensemble );
	}
	const Ensemble* ensemble() const noexcept
	{
		return _ensemble;
	}

	// Enabled/disables an axis
	void setAxisEnabled( Ensemble::VolumeID id, bool enabled )
	{
		bool updateVolumesBuffer = false;
		bool updateSelectionBuffer = false;

		const auto setAxisEnabledIntern = [&] ( Ensemble::VolumeID id, bool enabled )
		{
			auto& axis = _axes[id];
			if( axis->intervals().size() ) updateSelectionBuffer = true;

			if( enabled )
			{
				_region->enabledAxes().insert( id );
				const auto& volume = _ensemble->volume( id );
				if( &volume != &axis->volume() )
				{
					axis->setVolume( volume );
					updateVolumesBuffer = true;
				}
			}
			else _region->enabledAxes().erase( _region->enabledAxes().find( id ) );

			axis->setVisible( enabled );
		};

		if( id.type >= Ensemble::Derived::eHist1 && id.type <= Ensemble::Derived::eHist5 )
			for( auto type = Ensemble::Derived::eHist1; type <= Ensemble::Derived::eHist5; type = static_cast<Ensemble::Derived>( static_cast<int32_t>( type ) + 1 ) )
				setAxisEnabledIntern( Ensemble::VolumeID( id.field, type ), enabled );
		else setAxisEnabledIntern( id, enabled );

		if( updateVolumesBuffer ) this->updateVolumesBuffer();
		if( updateSelectionBuffer ) this->updateSelectionBuffer();
		this->updateVisibilityBuffer();

		// When disabling an axis, sometimes the lines don't get updated after the layout changes - so we listen to the scroll areas repaint event
		_updateWhenScrollAreaRepaints = true;
	}

	// Setter for the spacing between axes
	void setAxisSpacing( int spacing )
	{
		_updateWhenScrollAreaRepaints = true;
		_axesLayout->setSpacing( spacing );
	}

	// Getters for the maximum and current range of an axis
	vec2d maximumRange( Ensemble::VolumeID type ) const
	{
		if( auto it = _axes.find( type ); it != _axes.end() ) return it->second->maximumRange();
		else throw std::runtime_error( "ParallelCoordinates::maximumRange -> No maximum range available for requested id." );
	}
	vec2d currentRange( Ensemble::VolumeID type ) const
	{
		if( auto it = _axes.find( type ); it != _axes.end() ) return it->second->currentRange();
		else throw std::runtime_error( "ParallelCoordinates::maximumRange -> No current range available for requested id." );
	}

	// Setter for the number of voxels that are to be displayed
	void setSampleCount( int32_t sampleCount )
	{
		if( sampleCount != _sampleCount )
		{
			_sampleCount = sampleCount;
			this->update();
		}
	}

	// Notify the widget that the color of samples is starting/stopping to be edited
	void startEditingSampleColor( bool selected )
	{
		_editingSampleColor = true;
		_editingSelectedSampleColor = selected;
		emit colorChanged( selected ? _selectedSamplesColor : _unselectedSamplesColor );
		emit sampleColorsChanged( _unselectedSamplesColor, _selectedSamplesColor );
	}
	void stopEditingSampleColor()
	{
		_editingSampleColor = false;
	}

	// Return the color of samples and whether they are currently being edited
	std::pair<QColor, QColor> sampleColors() const noexcept
	{
		return std::make_pair( _unselectedSamplesColor, _selectedSamplesColor );
	}
	bool editingSampleColor() const noexcept
	{
		return _editingSampleColor;
	}

	// Setter for the color map used to color the lines
	void updateColorMap( const ColorMap1D* colorMap1D )
	{
		if( _colorMap1D ) QObject::disconnect( _colorMap1D, nullptr, this, nullptr );
		if( _colorMap2D ) QObject::disconnect( _colorMap1D, nullptr, this, nullptr );

		_colorMap1D = _colorMap1D == colorMap1D ? nullptr : colorMap1D;
		_colorMap2D = nullptr;

		if( _colorMap1D )
		{
			QObject::connect( _colorMap1D, &ColorMap1D::colorMapChanged, this, QOverload<>::of( &QWidget::update ) );
			QObject::connect( _colorMap1D, &ColorMap2D::destroyed, this, &ParallelCoordinates::resetColorMap );
		}
		this->update();
	}
	void updateColorMap( const ColorMap2D* colorMap2D )
	{
		if( _colorMap1D ) QObject::disconnect( _colorMap1D, nullptr, this, nullptr );
		if( _colorMap2D ) QObject::disconnect( _colorMap1D, nullptr, this, nullptr );

		_colorMap1D = nullptr;
		_colorMap2D = _colorMap2D == colorMap2D ? nullptr : colorMap2D;

		if( _colorMap2D )
		{
			QObject::connect( _colorMap2D, &ColorMap2D::colorMapChanged, this, QOverload<>::of( &QWidget::update ) );
			QObject::connect( _colorMap2D, &ColorMap2D::destroyed, this, &ParallelCoordinates::resetColorMap );
		}
		this->update();
	}

signals:
	// Signals when the ensemble changes
	void ensembleChanged( const Ensemble* );

	// Signals when the color of currently edited samples changes
	void colorChanged( QColor );

	// Signals when the color of samples changes
	void sampleColorsChanged( QColor, QColor );

	// Signals when the permutation buffer changes
	void permutationBufferChanged( GLint );

public slots:
	// Setter for enabling/disabling real time brushing
	void setRealtimeEnabled( bool enabled )
	{
		for( auto& [id, axis] : _axes ) axis->setRealtimeEnabled( enabled );
	}

	// Setter for the color of the unselected/selected samples
	void setColor( QColor color )
	{
		if( _editingSampleColor )
		{
			if( _editingSelectedSampleColor ) _selectedSamplesColor = color;
			else _unselectedSamplesColor = color;

			emit sampleColorsChanged( _unselectedSamplesColor, _selectedSamplesColor );
			this->update();
		}
	}

	// Clears all selected intervals
	void clearSelection()
	{
		if( _region )
		{
			for( auto& [key, intervals] : _region->intervals() ) intervals.clear();
			for( auto& [type, axis] : _axes ) axis->update();
			this->updateSelectionBuffer();
		}
	}

	 // Resets the current color map
	void resetColorMap()
	{
		if( _colorMap1D ) QObject::disconnect( _colorMap1D, nullptr, this, nullptr );
		if( _colorMap2D ) QObject::disconnect( _colorMap1D, nullptr, this, nullptr );

		_colorMap1D = nullptr;
		_colorMap2D = nullptr;

		this->update();
	}

private slots:
	// When the selection changes, update this widget and all axes
	void onSelectionChanged()
	{
		this->update();
		for( auto& [type, axis] : _axes ) axis->update();
	}

private:
	// Initialize some OpenGL objects
	void initializeGL() override
	{
		if( HMODULE module = GetModuleHandleA( "renderdoc.dll" ) )
		{
			pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI) GetProcAddress( module, "RENDERDOC_GetAPI" );
			RENDERDOC_GetAPI( eRENDERDOC_API_Version_1_1_2, (void**) &_renderDoc );
		}

		QOpenGLFunctions_4_5_Core::initializeOpenGLFunctions();

		_shader.addShaderFromSourceFile( QOpenGLShader::Vertex, ":/shaders/parallel_coordinates.vert" );
		_shader.addShaderFromSourceFile( QOpenGLShader::Fragment, ":/shaders/parallel_coordinates.frag" );
		_shader.link();

		_initialized = true;
	}

	// Render the parallel coordinates lines
	void paintGL() override
	{
		// Clear background
		glClearColor( 1.0f, 1.0f, 1.0f, 1.0f );
		glClear( GL_COLOR_BUFFER_BIT );

		if( !_ensemble ) return;
		if( !_volumesBuffer ) this->updateVolumesBuffer();
		if( _renderDoc && _captureFrame ) _renderDoc->StartFrameCapture( nullptr, nullptr );

		// Collect axes
		struct Axis
		{
			const ParallelCoordinatesAxis* axis = nullptr;
			Ensemble::VolumeID id;
			std::pair<QPoint, QPoint> axisPoints;

			struct
			{
				vec2f range;
				int index;
				float x;
			} uniform;
		};
		auto axes = std::vector<Axis>();
		for( const auto [id, axis] : _axes ) if( _region->enabledAxes().count( id ) )
			axes.push_back( Axis { axis, id, axis->mapAxesPoints( this ), 0.0f } );
		std::sort( axes.begin(), axes.end(), [] ( const Axis& a, const Axis& b ) { return a.axisPoints.first.x() < b.axisPoints.first.x(); } );

		// Remove axes after end of scroll area
		bool removeAxis = false;
		for( auto it = axes.begin(); it != axes.end(); )
		{
			if( it->axisPoints.first.x() > _scrollArea->rect().right() )
			{
				if( removeAxis ) it = axes.erase( it );
				else removeAxis = true, ++it;
			}
			else ++it;
		}
		if( axes.empty() ) return;

		// Set viewport and scissor
		const auto left = axes.front().axisPoints.first.x();
		const auto right = axes.back().axisPoints.second.x();
		const auto top = axes.front().axisPoints.first.y();
		const auto bottom = axes.front().axisPoints.second.y();
		glViewport( left, this->height() - bottom, right - left, bottom - top );

		// Calculate uniform data
		for( int32_t i = 0; i < axes.size(); ++i )
		{
			auto& axis = axes[i];
			axis.uniform.range = axis.axis->currentRange();
			axis.uniform.index = _volumeIndices[axis.axis];
			axis.uniform.x = static_cast<float>( axis.axisPoints.first.x() - left ) / ( right - left ) * 2.0f - 1.0f;
		}

		// Set uniforms
		_shader.bind();
		for( int32_t i = 0; i < axes.size(); ++i )
		{
			_shader.setUniformValue( ( "axes[" + std::to_string( i ) + "].range" ).data(), axes[i].uniform.range.x, axes[i].uniform.range.y );
			_shader.setUniformValue( ( "axes[" + std::to_string( i ) + "].volume" ).data(), axes[i].uniform.index );
			_shader.setUniformValue( ( "axes[" + std::to_string( i ) + "].x" ).data(), axes[i].uniform.x );
		}
		_shader.setUniformValue( "voxelCount", _ensemble->voxelCount() );

		// Use specified 1D or 2D color map to color samples
		if( _colorMap1D )
		{
			_shader.setUniformValue( "colorMapRanges[0]", static_cast<GLfloat>( _colorMap1D->domain().x ), static_cast<GLfloat>( _colorMap1D->domain().y ) );
			_shader.setUniformValue( "colorMapRanges[1]", 0.0f, 0.0f );
			_shader.setUniformValue( "colorMapVolumeIndices[0]", _volumeIndices[_axes[_colorMap1D->volumeID()]] );
			_shader.setUniformValue( "colorMapVolumeIndices[1]", -1 );
			_shader.setUniformValue( "useColorMap1D", true );
			_shader.setUniformValue( "useColorMap2D", false );

			_shader.setUniformValue( "colorMap1D", 0 );
			glActiveTexture( GL_TEXTURE0 );
			glBindTexture( GL_TEXTURE_1D, _colorMap1D->texture() );
		}
		else if( _colorMap2D )
		{
			_shader.setUniformValue( "colorMapRanges[0]", static_cast<GLfloat>( _colorMap2D->firstDomain().x ), static_cast<GLfloat>( _colorMap2D->firstDomain().y ) );
			_shader.setUniformValue( "colorMapRanges[1]", static_cast<GLfloat>( _colorMap2D->secondDomain().x ), static_cast<GLfloat>( _colorMap2D->secondDomain().y ) );
			_shader.setUniformValue( "colorMapVolumeIndices[0]", _volumeIndices[_axes[_colorMap2D->volumeIDs().first]] );
			_shader.setUniformValue( "colorMapVolumeIndices[1]", _volumeIndices[_axes[_colorMap2D->volumeIDs().second]] );
			_shader.setUniformValue( "useColorMap1D", false );
			_shader.setUniformValue( "useColorMap2D", true );

			_shader.setUniformValue( "colorMap2D", 1 );
			glActiveTexture( GL_TEXTURE1 );
			glBindTexture( GL_TEXTURE_2D, _colorMap2D->texture() );
		}
		else
		{
			_shader.setUniformValue( "useColorMap1D", false );
			_shader.setUniformValue( "useColorMap2D", false );
		}

		// Draw
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, _volumesBuffer );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, _visibilityBuffer );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, _region->selectionBuffer() );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, _permutationBuffer );

		// Draw unselected values
		_shader.setUniformValue( "requiredSelection", 0.0f );
		_shader.setUniformValue( "inColor", _unselectedSamplesColor );
		glDrawArraysInstanced( GL_LINE_STRIP, 0, axes.size(), _sampleCount );

		// Draw selected values
		_shader.setUniformValue( "requiredSelection", 1.0f );
		_shader.setUniformValue( "inColor", _selectedSamplesColor );
		glDrawArraysInstanced( GL_LINE_STRIP, 0, axes.size(), _sampleCount );

		glDisable( GL_BLEND );

		if( _renderDoc ) _renderDoc->EndFrameCapture( nullptr, nullptr ), _captureFrame = false;
	}

	// Handles keyboard events
	void keyPressEvent( QKeyEvent* event ) override
	{
		// Set flag to capture next frame
		if( event->key() == Qt::Key_C )
		{
			_captureFrame = true;
			this->update();
		}
	}

	// Updates the buffer that contains the values of all input volumes
	void updateVolumesBuffer( bool updateVisibilityBuffer = true, bool updateSelectionBuffer = true )
	{
		this->makeCurrent();
		if( !_volumesBuffer ) glGenBuffers( 1, &_volumesBuffer );
		glBindBuffer( GL_COPY_WRITE_BUFFER, _volumesBuffer );
		glBufferData( GL_COPY_WRITE_BUFFER, _ensemble->voxelCount() * _axes.size() * sizeof( float ), nullptr, GL_STATIC_DRAW );

		GLuint tmpbuffer = 0;
		glGenBuffers( 1, &tmpbuffer );
		glBindBuffer( GL_COPY_READ_BUFFER, tmpbuffer );

		glBindBuffer( GL_COPY_WRITE_BUFFER, _volumesBuffer );

		int32_t index = 0;
		_volumeIndices.clear();
		for( const auto [type, axis] : _axes )
		{
			if( _region->enabledAxes().count( type ) )
			{
				const auto offset = index * _ensemble->voxelCount() * sizeof( float );
				glBindBuffer( GL_COPY_READ_BUFFER, tmpbuffer );
				glBufferData( GL_COPY_READ_BUFFER, _ensemble->voxelCount() * sizeof( float ), axis->volume().data(), GL_STATIC_DRAW );
				glCopyBufferSubData( GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, offset, _ensemble->voxelCount() * sizeof( float ) );
			}

			_volumeIndices[axis] = index++;
		}

		if( !_permutationBuffer ) this->updatePermutationBuffer();
		if( updateVisibilityBuffer ) this->updateVisibilityBuffer();
		if( updateSelectionBuffer ) this->updateSelectionBuffer();
	}

	// Updates the buffer that contains what voxels should be rendered
	void updateVisibilityBuffer()
	{
		this->makeCurrent();
		if( !_visibilityBuffer ) glGenBuffers( 1, &_visibilityBuffer );

		auto visibility = Volume<uint32_t>( _ensemble->dimensions() );
		std::fill( visibility.begin(), visibility.end(), 1 );
		for( const auto [type, axis] : _axes ) if( _region->enabledAxes().count( type ) )
		{
			const auto range = axis->currentRange();
			const auto& volume = axis->volume();
			const auto domain = volume.domain();

			if( range.x > domain.x || range.y < domain.y )
			{
				util::compute_multi_threaded( 0, visibility.voxelCount(), [&] ( int32_t begin, int32_t end )
				{
					for( int32_t i = begin; i < end; ++i ) if( visibility.at( i ) )
					{
						const auto value = volume.at( i );
						if( value < range.x || value > range.y ) visibility.at( i ) = false;
					}
				} );
			}
		}

		glBindBuffer( GL_SHADER_STORAGE_BUFFER, _visibilityBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, visibility.voxelCount() * sizeof( uint32_t ), visibility.data(), GL_STATIC_DRAW );

		this->update();
	}

	// Updates the buffer that contains what voxels are selected
	void updateSelectionBuffer()
	{
		if( !_region ) return;

		this->makeCurrent();
		auto selection = Volume<float>( _ensemble->dimensions() );
		std::fill( selection.begin(), selection.end(), 2.0f );

		for( const auto [id, axis] : _axes ) if( _region->enabledAxes().count( id ) )
		{
			const auto& volume = axis->volume();
			const auto& intervals = axis->intervals();

			if( intervals.size() )
			{
				util::compute_multi_threaded( 0, selection.voxelCount(), [&] ( int32_t begin, int32_t end )
				{
					for( int32_t i = begin; i < end; ++i ) if( selection.at( i ) )
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
						selection.at( i ) = selected;
					}
				} );
			}
		}

		util::compute_multi_threaded( 0, selection.voxelCount(), [&] ( int32_t begin, int32_t end )
		{
			for( int32_t i = begin; i < end; ++i )
				if( selection.at( i ) == 2.0f ) selection.at( i ) = 0.0f;
		} );

		glBindBuffer( GL_SHADER_STORAGE_BUFFER, _region->selectionBuffer() );
		glBufferData( GL_SHADER_STORAGE_BUFFER, selection.voxelCount() * sizeof( float ), selection.data(), GL_STATIC_DRAW );

		_region->clearConstantMask();
		emit _region->selectionChanged();
		this->update();
	}

	// Updates the buffer that permutates the order of voxels
	void updatePermutationBuffer()
	{
		if( !_permutationBuffer ) glGenBuffers( 1, &_permutationBuffer );

		auto permutation = std::vector<GLint>( _ensemble->voxelCount() );
		std::iota( permutation.begin(), permutation.end(), 0 );

		auto generator = std::mt19937( std::random_device()( ) );
		std::shuffle( permutation.begin(), permutation.end(), generator );

		this->makeCurrent();
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, _permutationBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, permutation.size() * sizeof( GLint ), permutation.data(), GL_STATIC_DRAW );
		emit permutationBufferChanged( _permutationBuffer );

		this->update();
	}

	// Event filter for an edge case where the widget wasn't updated correctly
	bool eventFilter( QObject* object, QEvent* event ) override
	{
		if( object == _scrollArea && event->type() == QEvent::Resize )
		{
			_scrollArea->setMinimumHeight( _scrollAreaWidget->sizeHint().height() );
		}
		else if( _updateWhenScrollAreaRepaints && object == _scrollArea && event->type() == QEvent::Paint )
		{
			_updateWhenScrollAreaRepaints = false;
			this->update();
		}
		return false;
	}

	Region* _region = nullptr;
	const Ensemble* _ensemble = nullptr;

	QHBoxLayout* _axesLayout = nullptr;
	QWidget* _scrollAreaWidget = nullptr;
	QScrollArea* _scrollArea = nullptr;

	QHBoxLayout* _histogramLayout = nullptr;

	std::map<Ensemble::VolumeID, ParallelCoordinatesAxis*> _axes;

	bool _editingSampleColor = false, _editingSelectedSampleColor = false;
	QColor _unselectedSamplesColor = QColor( 50, 50, 50, 3 );
	QColor _selectedSamplesColor = QColor( 26, 115, 232, 5 );
	const ColorMap1D* _colorMap1D = nullptr;
	const ColorMap2D* _colorMap2D = nullptr;

	int32_t _sampleCount = 0;

	RENDERDOC_API_1_4_1* _renderDoc = nullptr;
	bool _initialized = false, _captureFrame = false, _updateWhenScrollAreaRepaints = false;

	QOpenGLShaderProgram _shader;
	std::unordered_map<const ParallelCoordinatesAxis*, int32_t> _volumeIndices;
	GLuint _volumesBuffer = 0, _visibilityBuffer = 0, _permutationBuffer = 0;
};
