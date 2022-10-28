#pragma once
#include <qopenglwidget.h>
#include <qopenglframebufferobject.h>
#include <qopenglfunctions.h>
#include <qopenglfunctions_4_5_core.h>
#include <qopenglshaderprogram.h>

#include <qformlayout.h>

#include <array>
#include <iostream>
#include <renderdoc_app.h>

#include "common_widgets.hpp"
#include "region.hpp"

// Class to manage a 1D color map
class ColorMap1D : public QWidget
{
	Q_OBJECT
public:
	static constexpr int32_t Size = 4096;

	// Class to manage a node for a 1D color map
	struct Node
	{
		double value;
		bool split;
		QColor left, right;
	};

	// Create a new color map for the specified volume type and domain
	ColorMap1D( QString name, Ensemble::VolumeID volumeID, const vec2d& domain, bool diverging = false ) : QWidget(), _name( std::move( name ) ), _volumeID( volumeID ), _domain( domain ),
		_colorMap( Size ), _image( Size, 2, QImage::Format_RGBA8888 )
	{
		const auto context = QOpenGLContext::globalShareContext();
		const auto functions = context->functions();

		functions->glGenTextures( 1, &_texture );
		functions->glBindTexture( GL_TEXTURE_1D, _texture );
		functions->glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MAX_LEVEL, 0 );
		functions->glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		functions->glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		functions->glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		functions->glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		functions->glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );

		this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );
		this->resetNodes( diverging );
	}

	// Copy a color map, giving it another name
	ColorMap1D( QString name, const ColorMap1D& other ) : QWidget(), _name( std::move( name ) ), _volumeID( other._volumeID ), _domain( other._domain ),
		_nodes( other._nodes ), _intervals( other._intervals ), _colorMap( other._colorMap ), _image( other._image )
	{
		const auto context = QOpenGLContext::globalShareContext();
		const auto functions = context->versionFunctions<QOpenGLFunctions_4_5_Core>();

		functions->glGenTextures( 1, &_texture );
		functions->glBindTexture( GL_TEXTURE_1D, _texture );
		functions->glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MAX_LEVEL, 0 );
		functions->glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		functions->glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		functions->glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		functions->glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		functions->glTexParameteri( GL_TEXTURE_1D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		functions->glTexImage1D( GL_TEXTURE_1D, 0, GL_RGBA32F, Size, 0, GL_RGBA, GL_FLOAT, _colorMap.data() );

		this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );
	}

	// Reset the nodes to the default (diverging) color map
	void resetNodes( bool diverging )
	{
		_nodes.clear();

		if( diverging )
		{
			const auto lower = QColor( 0.23 * 255, 0.299 * 255, 0.754 * 255, 255 );
			const auto middle = QColor( 0.865 * 255, 0.865 * 255, 0.865 * 255, 0 );
			const auto upper = QColor( 0.706 * 255, 0.016 * 255, 0.15 * 255, 255 );

			_nodes = std::vector<Node> {
				Node { _domain.x, false, lower, lower },
				Node { ( _domain.x + _domain.y ) / 2.0, false, middle, middle },
				Node { _domain.y, false, upper, upper }
			};
		}
		else
		{
			const std::array<vec3f, 8> colors { // parula color map https://www.mathworks.com/help/matlab/ref/parula.html
				vec3f( 0.2422f, 0.1504f, 0.6603f ),
				vec3f( 0.2810f, 0.3228f, 0.9579f ),
				vec3f( 0.1786f, 0.5289f, 0.9682f ),
				vec3f( 0.0689f, 0.6948f, 0.8394f ),
				vec3f( 0.2161f, 0.7843f, 0.5923f ),
				vec3f( 0.6720f, 0.7793f, 0.2227f ),
				vec3f( 0.9970f, 0.7659f, 0.2199f ),
				vec3f( 0.9769f, 0.9839f, 0.0805f )
			};

			const auto step = ( _domain.y - _domain.x ) / ( colors.size() - 1 );
			for( int32_t i = 0; i < colors.size(); ++i )
			{
				const auto x = ( i == colors.size() - 1 ) ? _domain.y : _domain.x + step * i;

				const auto a = ( x - _domain.x ) / ( _domain.y - _domain.x ) * 255.0;
				const auto color = QColor( colors[i].x * 255.0, colors[i].y * 255.0, colors[i].z * 255.0, a );

				_nodes.push_back( Node { x, false, color, color } );
			}
		}

		this->updateColorMap();
	}

	// Setter and getter for the name
	void setName( QString name )
	{
		_name = std::move( name );
		emit nameChanged( _name );
	}
	const QString& name() const noexcept
	{
		return _name;
	}

	// Getters for the volume type and domain
	Ensemble::VolumeID volumeID() const noexcept
	{
		return _volumeID;
	}
	vec2d domain() const noexcept
	{
		return _domain;
	}

	// Getter for the nodes
	const std::vector<Node>& nodes() const noexcept
	{
		return _nodes;
	}
	std::vector<Node>& nodes() noexcept
	{
		return _nodes;
	}

	// Getter for the selected intervals (for thresholding)
	const std::vector<vec2d>& intervals() noexcept
	{
		return _intervals;
	}

	// Getters for different representations of the actual color map
	const std::vector<vec4f>& colorMap() const noexcept
	{
		return _colorMap;
	}
	const QImage& image() const noexcept
	{
		return _image;
	}
	GLuint texture() const noexcept
	{
		return _texture;
	}

signals:
	void colorMapChanged();
	void nameChanged( const QString& );
	void intervalsChanged( const std::vector<vec2d>& );

public slots:
	void updateColorMap()
	{
		// Fill color map
		const auto diff = ( _domain.y - _domain.x );
		auto previousColor = _nodes.front().left;
		auto previousIndex = -1;
		for( int32_t i = 0; i < _nodes.size(); ++i )
		{
			const auto& node = _nodes[i];
			const auto x = diff ? ( node.value - _domain.x ) / diff : 0.0;
			const auto index = static_cast<int32_t>( x * ( _colorMap.size() - 1 ) );

			for( int32_t j = previousIndex + 1; j < index; ++j )
			{
				const auto x = static_cast<double>( j - previousIndex ) / ( index - previousIndex );

				const auto red = ( 1.0 - x ) * previousColor.red() + x * node.left.red();
				const auto green = ( 1.0 - x ) * previousColor.green() + x * node.left.green();
				const auto blue = ( 1.0 - x ) * previousColor.blue() + x * node.left.blue();
				const auto alpha = ( 1.0 - x ) * previousColor.alpha() + x * node.left.alpha();

				_colorMap[j] = vec4f( red, green, blue, alpha ) / 255.0f;
			}

			const auto color = node.left;
			_colorMap[index] = vec4f( color.red(), color.green(), color.blue(), color.alpha() ) / 255.0f;

			if( node.split && index < _colorMap.size() - 1 )
			{
				const auto color = node.right;
				_colorMap[index] = vec4f( color.red(), color.green(), color.blue(), color.alpha() ) / 255.0f;
			}

			previousColor = node.right;
			previousIndex = index;
		}
		for( int32_t i = previousIndex + 1; i < _colorMap.size(); ++i )
		{
			const auto color = _nodes.back().right;
			_colorMap[i] = vec4f( color.red(), color.green(), color.blue(), color.alpha() ) / 255.0f;
		}

		// Remove intervals
		for( const auto interval : _intervals )
		{
			const auto xlower = ( interval.x - _domain.x ) / ( _domain.y - _domain.x );
			const auto xupper = ( interval.y - _domain.x ) / ( _domain.y - _domain.x );

			const auto lower = static_cast<int32_t>( std::floor( xlower * ( _colorMap.size() - 1 ) ) );
			const auto upper = static_cast<int32_t>( std::ceil( xupper * ( _colorMap.size() - 1 ) ) );

			for( int32_t i = lower; i <= upper; ++i ) _colorMap[i] = vec4f( 1.0f, 1.0f, 1.0f, 0.0f );
		}

		// Copy to QImage
		for( int32_t i = 0; i < _colorMap.size(); ++i )
		{
			const auto color = _colorMap[i] * 255.0;
			_image.setPixelColor( i, 0, QColor( color.x, color.y, color.z, 255 ) );
			_image.setPixelColor( i, 1, QColor( color.x, color.y, color.z, color.w ) );
		}

		// Upload to texture
		const auto context = QOpenGLContext::globalShareContext();
		const auto functions = context->versionFunctions<QOpenGLFunctions_4_5_Core>();
		functions->glBindTexture( GL_TEXTURE_1D, _texture );
		functions->glTexImage1D( GL_TEXTURE_1D, 0, GL_RGBA32F, Size, 0, GL_RGBA, GL_FLOAT, _colorMap.data() );

		emit colorMapChanged();
		this->update();
	}
	void setIntervals( std::vector<vec2d> intervals )
	{
		emit intervalsChanged( _intervals = std::move( intervals ) );
		this->updateColorMap();
	}

private:
	QSize sizeHint() const override
	{
		return QSize( 0, 20 );
	}
	void paintEvent( QPaintEvent* event ) override
	{
		auto painter = QPainter( this );
		painter.setRenderHint( QPainter::Antialiasing, true );
		painter.drawImage( this->rect(), _image );

		painter.setBrush( Qt::transparent );
		painter.setPen( QColor( 218, 220, 224 ) );
		painter.drawRect( this->rect() );
	}

	QString _name;
	Ensemble::VolumeID _volumeID;

	const vec2d& _domain;
	std::vector<Node> _nodes;
	std::vector<vec2d> _intervals;

	std::vector<vec4f> _colorMap;
	QImage _image;
	GLuint _texture = 0;
};

// Class for the node editor of the 1D color map editor
class ColorMap1DNodeEditor : public QWidget
{
	Q_OBJECT
public:
	// Default constructor :)
	ColorMap1DNodeEditor() : QWidget()
	{
		this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );
		this->setFocusPolicy( Qt::WheelFocus );
		this->setMouseTracking( true );
	}

	// Return the underlying 1Dcolor map
	ColorMap1D* colorMap() const noexcept
	{
		return _colorMap;
	}

	// Return the input volume
	const Volume<float>* volume() const noexcept
	{
		return _volume;
	}

	// Return the color of the currently selected node
	QColor color() const
	{
		if( _selectedNode )
		{
			if( _selectedSide == Side::eLeft ) return _selectedNode->left;
			else return _selectedNode->right;
		}
		else return QColor( 0, 0, 0, 0 );
	}

	// Returns the currently used mask volume
	std::shared_ptr<Volume<float>> mask() const noexcept
	{
		return _mask;
	}

signals:
	// Signal for when the horizontal hovered position changes
	void hoveredValueChanged( double );

	// Signal for when the horizontal position of the currently selected node changes
	void selectedNodeValueChanged( double );

	// Signal for when the color of the currently selected node changes
	void colorChanged( QColor );

public slots:
	// Setter for the underlying 1D color map
	void setColorMap( ColorMap1D& colorMap )
	{
		if( _colorMap ) QObject::disconnect( _colorMap, nullptr, this, nullptr );

		_colorMap = &colorMap;
		_hoveredNode = _selectedNode = nullptr;
		_hoveredSide = _selectedSide = Side::eNone;

		QObject::connect( _colorMap, &ColorMap1D::colorMapChanged, [=] { this->update(); } );
		this->update();
	}

	// Setter for the input volume
	void setVolume( const Volume<float>* volume )
	{
		if( _volume != volume )
		{
			_volume = volume;
			this->updateHistogram();
		}
	}

	// Setters for the horizontal position (value) and color of the currently selected node
	void setSelectedNodeValue( double value )
	{
		if( _selectedNode && _selectedNode->value != value )
		{
			emit selectedNodeValueChanged( _selectedNode->value = value );

			// Swap node to left if neccessary
			auto pred = ( _selectedNode - 1 );
			while( pred >= _colorMap->nodes().data() && _selectedNode->value < pred->value )
			{
				std::swap( *_selectedNode, *pred );
				_selectedNode = pred--;
			}

			// Swap node to right if neccessary
			auto succ = ( _selectedNode + 1 );
			while( succ < _colorMap->nodes().data() + _colorMap->nodes().size() && _selectedNode->value > succ->value )
			{
				std::swap( *_selectedNode, *succ );
				_selectedNode = succ++;
			}

			_colorMap->updateColorMap();
		}
	}
	void setColor( QColor color )
	{
		if( _selectedNode )
		{
			if( _selectedNode->split )
			{
				if( _selectedSide == Side::eLeft ) _selectedNode->left = color;
				else _selectedNode->right = color;
			}
			else _selectedNode->left = _selectedNode->right = color;

			emit colorChanged( color );
			_colorMap->updateColorMap();
		}
	}

	// Setter for the mask
	void setMask( std::shared_ptr<Volume<float>> mask )
	{
		_mask = mask;
		this->updateHistogram();
	}

	// Deselects the currently selected node
	void deselectNode()
	{
		_selectedNode = nullptr;
		_selectedSide = Side::eNone;
		emit selectedNodeValueChanged( std::numeric_limits<double>::infinity() );
		this->update();
	}

private:
	// Gives a hint about this widget's preferred/minimum size
	QSize sizeHint() const override
	{
		return QSize( 100, 256 + 20 );
	}

	// Function to draw the nodes, histogram etc.
	void paintEvent( QPaintEvent* event ) override
	{
		// Fill background
		auto painter = QPainter( this );
		const auto rect = this->rect().marginsRemoved( QMargins( Padding, Padding, Padding, Padding ) );
		painter.fillRect( rect, QColor( 255, 255, 255 ) );
		if( !_colorMap ) return;

		// Draw intervals
		painter.setRenderHint( QPainter::Antialiasing, true );
		for( const auto interval : _colorMap->intervals() )
		{
			if( interval.x > _colorMap->domain().y || interval.y < _colorMap->domain().x ) continue;
			const auto begin = this->valueToX( interval.x );
			const auto end = this->valueToX( interval.y );

			const auto left = std::clamp( begin, rect.left(), rect.right() );
			const auto right = std::clamp( end, rect.left(), rect.right() );

			painter.fillRect( QRect( left, rect.top(), right - left, rect.height() ), QColor( 240, 240, 240 ) );

			painter.setPen( QPen( QColor( "#5f6368" ), 1.0, Qt::DashLine ) );
			painter.drawLine( left, rect.top(), left, rect.bottom() );
			painter.drawLine( right, rect.top(), right, rect.bottom() );
		}

		// Draw histogram
		if( !_histogram.size().isEmpty() ) painter.drawImage( rect, _histogram );

		// Draw lines
		const auto drawLine = [&] ( QPointF first, QPointF second )
		{
			if( first.x() > rect.right() || second.x() < rect.left() ) return;

			if( first.x() < rect.left() )
				QLineF( first, second ).intersect( QLineF( rect.left(), rect.top(), rect.left(), rect.bottom() ), &first );

			if( second.x() > rect.right() )
				QLineF( first, second ).intersect( QLineF( rect.right(), rect.top(), rect.right(), rect.bottom() ), &second );

			painter.setPen( QColor( 218, 220, 224 ) );
			painter.drawLine( first, second );
		};

		auto previous = QPoint( Padding, this->nodeToPoint( _colorMap->nodes().front() )[1] );
		for( const auto& node : _colorMap->nodes() )
		{
			const auto [x, yleft, yright] = this->nodeToPoint( node );
			drawLine( previous, QPointF( x, yleft ) );
			previous = QPoint( x, yright );
		}
		drawLine( previous, QPointF( rect.right(), this->nodeToPoint( _colorMap->nodes().back() )[2] ) );

		// Draw border
		painter.setBrush( Qt::transparent );
		painter.setPen( QColor( "#5f6368" ) );
		painter.drawRect( rect );

		// Draw nodes
		for( const auto& node : _colorMap->nodes() )
		{
			if( node.value < _colorMap->domain().x || node.value > _colorMap->domain().y ) continue;
			const auto [x, yleft, yright] = this->nodeToPoint( node );

			auto leftRGB = node.left;
			leftRGB.setAlpha( 255 );
			auto rightRGB = node.right;
			rightRGB.setAlpha( 255 );

			const auto size = ( &node == _hoveredNode || &node == _selectedNode ) ? 16 : 12;

			int32_t sizeLeft = 12, sizeRight = 12;
			if( node.split )
			{
				if( &node == _hoveredNode )
				{
					if( _hoveredSide == Side::eLeft ) sizeLeft = 16;
					else sizeRight = 16;
				}
				if( &node == _selectedNode )
				{
					if( _selectedSide == Side::eLeft ) sizeLeft = 16;
					else sizeRight = 16;
				}
			}
			else if( &node == _hoveredNode || &node == _selectedNode )
				sizeLeft = sizeRight = 16;

			const auto radiusLeft = sizeLeft / 2, radiusRight = sizeRight / 2;

			if( node.split )
			{
				painter.setBrush( leftRGB );
				painter.setPen( QPen( QColor( 218, 220, 224 ), 1.0 ) );
				painter.drawChord( QRect( x - radiusLeft, yleft - radiusLeft, sizeLeft, sizeLeft ), 90 * 16, 180 * 16 );

				painter.setBrush( rightRGB );
				painter.setPen( QPen( QColor( 218, 220, 224 ), 1.0 ) );
				painter.drawChord( QRect( x - radiusRight, yright - radiusRight, sizeRight, sizeRight ), 90 * -16, 180 * 16 );
			}
			else
			{
				painter.setBrush( leftRGB );
				painter.setPen( QPen( QColor( 218, 220, 224 ), 1.0 ) );
				painter.drawEllipse( QPoint( x, yleft ), radiusLeft, radiusLeft );
			}
		}
	}

	// Handles mouse move events
	void mouseMoveEvent( QMouseEvent* event ) override
	{
		if( event->buttons() == Qt::NoButton ) this->updateHoveredNode( event->pos() );
		else if( event->buttons() == Qt::LeftButton )
		{
			if( _hoveredNode )
			{
				const auto [x, alpha] = this->pointToNode( event->pos() );

				// Update node position & alpha
				_hoveredNode->value = x;
				if( _hoveredNode->split )
				{
					if( _hoveredSide == Side::eLeft ) _hoveredNode->left.setAlpha( alpha );
					else _hoveredNode->right.setAlpha( alpha );
				}
				else
				{
					_hoveredNode->left.setAlpha( alpha );
					_hoveredNode->right.setAlpha( alpha );
				}

				// Swap node to left if neccessary
				auto pred = ( _hoveredNode - 1 );
				while( pred >= _colorMap->nodes().data() && _hoveredNode->value < pred->value )
				{
					std::swap( *_hoveredNode, *pred );
					_hoveredNode = pred--;
				}

				// Swap node to right if neccessary
				auto succ = ( _hoveredNode + 1 );
				while( succ < _colorMap->nodes().data() + _colorMap->nodes().size() && _hoveredNode->value > succ->value )
				{
					std::swap( *_hoveredNode, *succ );
					_hoveredNode = succ++;
				}

				// Hovered node was selected before, so it should be after sorting
				_selectedNode = _hoveredNode;

				if( _selectedNode ) emit colorChanged( this->color() );
				emit selectedNodeValueChanged( _selectedNode ? _selectedNode->value : std::numeric_limits<double>::infinity() );
				emit hoveredValueChanged( _hoveredNode->value );
				_colorMap->updateColorMap();
			}
		}
	}

	// Reset the hovered node when the mouse leaves
	void leaveEvent( QEvent* event ) override
	{
		_hoveredNode = nullptr;
		emit hoveredValueChanged( std::numeric_limits<double>::infinity() );
		this->update();
	}

	// Handles mouse button events
	void mousePressEvent( QMouseEvent* event ) override
	{
		if( event->buttons() == Qt::LeftButton )
		{
			if( _hoveredNode )
			{
				_selectedNode = _hoveredNode;
				_selectedSide = _hoveredSide;
			}
			else
			{
				const auto [x, alpha] = this->pointToNode( event->pos() );

				auto node = ColorMap1D::Node { x, false, QColor(), QColor() };
				auto lower = std::lower_bound( _colorMap->nodes().begin(), _colorMap->nodes().end(), node, [] ( const auto& a, const auto& b )
				{ return a.value < b.value; } );
				auto upper = std::upper_bound( _colorMap->nodes().begin(), _colorMap->nodes().end(), node, [] ( const auto& a, const auto& b )
				{ return a.value < b.value; } );

				if( lower == _colorMap->nodes().begin() ) lower =  _colorMap->nodes().end();
				else --lower;

				if( lower == _colorMap->nodes().end() )
					node.left = node.right = upper->left;
				else if( upper == _colorMap->nodes().end() )
					node.left = node.right = lower->right;
				else
				{
					const auto lowerColor = lower->right;
					const auto upperColor = upper->left;
					const auto interp = ( x - lower->value ) / ( upper->value - lower->value );

					const auto red = lowerColor.red() + interp * ( upperColor.red() - lowerColor.red() );
					const auto green = lowerColor.green() + interp * ( upperColor.green() - lowerColor.green() );
					const auto blue = lowerColor.blue() + interp * ( upperColor.blue() - lowerColor.blue() );

					node.left = node.right = QColor( red, green, blue );
				}

				node.left.setAlpha( alpha );
				node.right.setAlpha( alpha );
				auto it =  _colorMap->nodes().insert( upper, std::move( node ) );

				_hoveredNode = _selectedNode = it.operator->();
			}

			if( _selectedNode ) emit colorChanged( this->color() );
			emit selectedNodeValueChanged( _selectedNode ? _selectedNode->value : std::numeric_limits<double>::infinity() );
			_colorMap->updateColorMap();
		}
		else if( event->buttons() == Qt::RightButton )
		{
			if( _hoveredNode && _colorMap->nodes().size() > 1 )
			{
				const auto hoveredNodeIndex = _hoveredNode - _colorMap->nodes().data();
				const auto selectedNodeIndex = _selectedNode - _colorMap->nodes().data();

				_colorMap->nodes().erase( _colorMap->nodes().begin() + hoveredNodeIndex );

				if( _selectedNode )
				{
					if( _selectedNode == _hoveredNode )
					{
						_selectedNode = nullptr;
						if( _selectedNode ) emit colorChanged( this->color() );
						emit selectedNodeValueChanged( _selectedNode ? _selectedNode->value : std::numeric_limits<double>::infinity() );
					}
					else _selectedNode = _colorMap->nodes().data() + selectedNodeIndex;
				}
				this->updateHoveredNode( event->pos() );
				_colorMap->updateColorMap();
			}
		}
		else if( event->button() == Qt::MiddleButton )
		{
			if( _hoveredNode )
			{
				if( _hoveredNode->split )
				{
					if( _hoveredSide == Side::eLeft ) _hoveredNode->right = _hoveredNode->left;
					else _hoveredNode->left = _hoveredNode->right;
					_hoveredNode->split = false;
				}
				else _hoveredNode->split = true;

				if( _selectedNode ) emit colorChanged( this->color() );
				this->update();
			}
		}
	}

	// Handles key events
	void keyPressEvent( QKeyEvent* event ) override
	{
		// Resets the nodes to the default
		if( event->key() == Qt::Key_R )
		{
			_colorMap->resetNodes( _colorMap->volumeID().difference );
			_hoveredNode = _selectedNode = nullptr;
			_hoveredSide = _selectedSide = Side::eNone;
			emit selectedNodeValueChanged( std::numeric_limits<double>::infinity() );
		}
		else if( event->key() == Qt::Key_L )
		{
			_logarithmicHistogram = !_logarithmicHistogram;
			this->updateHistogram();
		}
	}

	// Updates the hovered node given the cursor position
	void updateHoveredNode( QPoint cursor )
	{
		struct { ColorMap1D::Node* node; Side side; double distance; } closest { nullptr, Side::eNone, 10.0 };
		for( auto& node : _colorMap->nodes() )
		{
			const auto [x, yleft, yright] = this->nodeToPoint( node );

			const auto distanceLeft = QLineF( cursor, QPoint( x, yleft ) ).length();
			const auto distanceRight = QLineF( cursor, QPoint( x, yright ) ).length();

			if( distanceLeft < closest.distance ) closest = { &node, Side::eLeft, distanceLeft };
			if( distanceRight < closest.distance ) closest = { &node, Side::eRight, distanceRight };

			if( node.split && closest.node == &node && distanceLeft == distanceRight )
				closest.side = cursor.x() < x ? Side::eLeft : Side::eRight;
		}

		if( closest.node != _hoveredNode || closest.side != _hoveredSide )
		{
			_hoveredNode = closest.node;
			_hoveredSide = closest.side;
			this->update();
		}

		emit hoveredValueChanged( this->pointToNode( cursor ).first );
	}

	// Recomputes the background histogram
	void updateHistogram()
	{
		if( _volume && _mask )
		{
			auto counters = std::vector<std::pair<double, double>>( 100 );
			for( size_t i = 0; i < _volume->voxelCount(); ++i )
			{
				const auto value = _volume->at( i );
				const auto x = ( value - _colorMap->domain().x ) / ( _colorMap->domain().y - _colorMap->domain().x );
				const auto index = std::clamp( static_cast<int32_t>( x * counters.size() ), 0, static_cast<int32_t>( counters.size() - 1 ) );
				if( _mask->at( i ) ) ++counters[index].first;
				++counters[index].second;
			}

			if( _logarithmicHistogram )
			{
				const auto max = std::log10( _volume->voxelCount() + 1 );
				for( auto& [first, second] : counters )
				{
					first = std::log10( first + 1 ) / max;
					second = std::log10( second + 1 ) / max;
				}
			}
			else
			{
				const auto max = std::log10( _volume->voxelCount() + 1 );
				for( auto& [first, second] : counters )
				{
					first = first / _volume->voxelCount();
					second = second / _volume->voxelCount();
				}
			}

			_histogram = QImage( counters.size(), 100, QImage::Format_RGBA8888 );
			_histogram.fill( QColor( 255, 255, 255, 0 ) );
			for( size_t x = 0; x < _histogram.width(); ++x )
			{
				int32_t y = 0;
				for( ; y < counters[x].first * _histogram.height(); ++y ) _histogram.setPixelColor( x, y, QColor( 200, 222, 249, 100 ) );
				for( ; y < counters[x].second * _histogram.height(); ++y ) _histogram.setPixelColor( x, y, QColor( 200, 200, 200, 100 ) );
			}

			_histogram = _histogram.mirrored();
		}
		else _histogram = QImage();

		this->update();
	}

	// Helper functions to map a value to an x-coordinate and alpha value to a y-coordinate
	int valueToX( double value ) const
	{
		return Padding + ( value - _colorMap->domain().x ) / ( _colorMap->domain().y - _colorMap->domain().x ) * ( this->width() - 2 * Padding );
	}
	int valueToY( double value ) const
	{
		return Padding + ( 1.0 - value / 255.0 ) * ( this->height() - 2 * Padding );
	}

	// Helper functions to map screen points to nodes and vice evrsa
	std::pair<double, int> pointToNode( QPoint point ) const
	{
		const auto x = _colorMap->domain().x + std::clamp( static_cast<double>( point.x() - Padding ) / ( this->width() - 2 * Padding ), 0.0, 1.0 ) * ( _colorMap->domain().y - _colorMap->domain().x );
		const auto y = 255.0 * ( 1.0 - std::clamp( static_cast<double>( point.y() - Padding ) / ( this->height() - 2 * Padding ), 0.0, 1.0 ) );
		return std::pair<double, int>( x, static_cast<int>( y ) );
	}
	std::array<int, 3> nodeToPoint( const ColorMap1D::Node& node ) const
	{
		const auto x = this->valueToX( node.value );
		const auto yleft = this->valueToY( node.left.alpha() );
		const auto yright = this->valueToY( node.right.alpha() );
		return std::array<int, 3> { x, yleft, yright };
	}

	static constexpr int32_t Padding = 10;

	ColorMap1D* _colorMap = nullptr;
	const Volume<float>* _volume = nullptr;
	std::shared_ptr<Volume<float>> _mask;
	QImage _histogram;
	bool _logarithmicHistogram = true;

	ColorMap1D::Node* _hoveredNode = nullptr;
	ColorMap1D::Node* _selectedNode = nullptr;
	enum class Side { eNone, eLeft, eRight } _hoveredSide { Side::eNone }, _selectedSide { Side::eNone };
};

// Class to edit a 1D color map
class ColorMap1DEditor : public QWidget
{
	Q_OBJECT
public:
	// Default constructor :)
	ColorMap1DEditor() : QWidget()
	{
		// Initialize and layout widgets
		auto layout = new QGridLayout( this );
		layout->setContentsMargins( 0, 0, 0, 0 );
		layout->setSpacing( 0 );
		layout->setColumnStretch( 1, 1 );
		layout->setColumnStretch( 3, 1 );

		layout->addWidget( _nodeEditor = new ColorMap1DNodeEditor, 0, 0, 1, -1 );
		layout->addWidget( _axisBar = new ParallelCoordinatesAxisBar( ParallelCoordinatesAxisBar::Direction::eHorizontal, _intervals, vec2d( 0.0, 1.0 ), 1 ), 1, 0, 1, -1 );
		layout->addWidget( _lowerLabel = new QLabel, 2, 0, 1, 1 );
		layout->addWidget( new QWidget, 2, 1, 1, 1 );
		layout->addWidget( _currentValue = new QDoubleSpinBox, 2, 2, 1, 1 );
		layout->addWidget( new QWidget, 2, 3, 1, 1 );
		layout->addWidget( _upperLabel = new QLabel, 2, 4, 1, 1 );

		_axisBar->setZoomingEnabled( false );
		_axisBar->setRealtimeEnabled( true );

		_lowerLabel->setAlignment( Qt::AlignLeft | Qt::AlignVCenter );
		_upperLabel->setAlignment( Qt::AlignRight | Qt::AlignVCenter );

		_currentValue->setButtonSymbols( QSpinBox::NoButtons );
		_currentValue->setVisible( false );

		// Initialize connections
		QObject::connect( _axisBar, &ParallelCoordinatesAxisBar::intervalsChanged, [=]
		{
			this->colorMap()->setIntervals( _axisBar->intervals().empty() ? std::vector<vec2d>() : _axisBar->invertedIntervals() );
		} );
		QObject::connect( _nodeEditor, &ColorMap1DNodeEditor::colorChanged, this, &ColorMap1DEditor::colorChanged );
		QObject::connect( _nodeEditor, &ColorMap1DNodeEditor::selectedNodeValueChanged, [=] ( double value )
		{
			_currentValue->setVisible( value != std::numeric_limits<double>::infinity() );
			_currentValue->setValue( value );
		} );
		QObject::connect( _nodeEditor, &ColorMap1DNodeEditor::hoveredValueChanged, [=] ( double value )
		{
			_axisBar->setHighlightedValue( value );
		} );
		QObject::connect( _currentValue, QOverload<double>::of( &QDoubleSpinBox::valueChanged ), [=] ( double value )
		{
			_nodeEditor->setSelectedNodeValue( value );
		} );
	}

	// Getter for the color map that is currently begin edited
	ColorMap1D* colorMap() const noexcept
	{
		return _nodeEditor->colorMap();
	}

	// Getter for the current input volume
	const Volume<float>* volume() const noexcept
	{
		return _nodeEditor->volume();
	}

	// Getter for the color of the currently selected node
	QColor color() const
	{
		return _nodeEditor->color();
	}

	// Getter for the current mask
	std::shared_ptr<Volume<float>> mask() const noexcept
	{
		return _nodeEditor->mask();
	}

signals:
	// Signals when the color of the currently selected node changes
	void colorChanged( QColor );

public slots:
	// Setter for the color map that is to be edited
	void setColorMap( ColorMap1D& colorMap )
	{
		_nodeEditor->setColorMap( colorMap );
		this->updateDomain();

		_axisBar->setIntervals( _intervals = colorMap.intervals() );
		if( _axisBar->intervals().size() ) _axisBar->invertIntervals();
	}

	// Setter for the input volume
	void setVolume( const Volume<float>* volume )
	{
		_nodeEditor->setVolume( volume );
		this->updateDomain();
	}

	// Setter for the color of the currently selected node
	void setColor( QColor color )
	{
		_nodeEditor->setColor( color );
	}

	// Setter for the current mask
	void setMask( std::shared_ptr<Volume<float>> mask )
	{
		_nodeEditor->setMask( mask );
	}

	// Deselects the currently selected node
	void deselectNode()
	{
		_nodeEditor->deselectNode();
		_currentValue->setVisible( false );
	}

private:
	// Updates the current domain and the corresponding widgets
	void updateDomain()
	{
		const auto domain = this->colorMap()->domain();
		auto onePercentStep = ( domain.y - domain.x ) / 100.0;
		auto singleStep = 1.0;
		auto precision = 1;
		while( onePercentStep && onePercentStep < 1.0 ) onePercentStep *= 10.0, singleStep /= 10.0, ++precision;

		_lowerLabel->setText( QString::number( domain.x, 'f', precision ) );
		_upperLabel->setText( QString::number( domain.y, 'f', precision ) );

		_currentValue->setSingleStep( singleStep );
		_currentValue->setDecimals( precision );
		_currentValue->setRange( domain.x, domain.y );

		_axisBar->setMaximumRange( domain, true );
		_axisBar->setPrecision( precision );
	}

	ColorMap1DNodeEditor* _nodeEditor = nullptr;
	ParallelCoordinatesAxisBar* _axisBar = nullptr;
	QLabel* _lowerLabel = nullptr;
	QLabel* _upperLabel = nullptr;
	QDoubleSpinBox* _currentValue = nullptr;

	std::vector<vec2d> _intervals;
};



// Class to manage a 2D color map
class ColorMap2D : public QObject
{
	Q_OBJECT
public:
	static constexpr int32_t Size = 1024;

	// Class to manage a polygon for a 2D color map
	struct Polygon
	{
		QPolygon screenPoints;
		std::vector<vec2f> points;
		GLuint buffer = 0;
		vec4f color;
	};

	// Create a new color map using the specified volume types and domains
	ColorMap2D( QString name, Ensemble::VolumeID firstVolumeID, Ensemble::VolumeID secondVolumeID, vec2d firstDomain, vec2d secondDomain ) :
		_name( std::move( name ) ), _volumeIDs( firstVolumeID, secondVolumeID ), _firstDomain( firstDomain ), _secondDomain( secondDomain )
	{
		const auto context = QOpenGLContext::globalShareContext();
		const auto functions = context->functions();
		functions->glGenTextures( 1, &_colorMap );
		functions->glBindTexture( GL_TEXTURE_2D, _colorMap );
		functions->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		functions->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		functions->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		functions->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		functions->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		functions->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		functions->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, Size, Size, 0, GL_RGBA, GL_FLOAT, nullptr );

		if( firstVolumeID.difference && secondVolumeID.difference ) this->setBackgrond( 50.0f );
	}

	// Copy a color map, giving it another name
	ColorMap2D( QString name, const ColorMap2D& other ) : _name( std::move( name ) ), _volumeIDs( other._volumeIDs ),
		_firstDomain( other._firstDomain ), _secondDomain( other._secondDomain ), _polygons( other._polygons )
	{
		const auto context = QOpenGLContext::globalShareContext();
		const auto functions = context->versionFunctions<QOpenGLFunctions_4_5_Core>();

		for( auto& polygon : _polygons )
		{
			functions->glGenBuffers( 1, &polygon.buffer );
			functions->glBindBuffer( GL_SHADER_STORAGE_BUFFER, polygon.buffer );
			functions->glBufferData( GL_SHADER_STORAGE_BUFFER, polygon.points.size() * sizeof( vec2f ), polygon.points.data(), GL_STATIC_DRAW );
		}

		functions->glGenTextures( 1, &_colorMap );
		functions->glBindTexture( GL_TEXTURE_2D, _colorMap );
		functions->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		functions->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		functions->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		functions->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		functions->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		functions->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		functions->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, Size, Size, 0, GL_RGBA, GL_FLOAT, nullptr );
		functions->glCopyImageSubData( other._colorMap, GL_TEXTURE_2D, 0, 0, 0, 0, _colorMap, GL_TEXTURE_2D, 0, 0, 0, 0, Size, Size, 1 );
	}

	// Setter and getter for the name
	void setName( QString name )
	{
		_name = std::move( name );
		emit nameChanged( _name );
	}
	const QString& name() const noexcept
	{
		return _name;
	}

	// Setter and getter for the background color slice (only applies when both volume types are difference volumes)
	void setBackgrond( float lightness )
	{
		_backgroundLightness = lightness;

		auto pixels = std::vector<vec4f>( 1024 * 1024 );
		for( int32_t x = 0; x < 1024; ++x )
		{
			for( int32_t y = 0; y < 1024; ++y )
			{
				const auto a = -110.0f + ( x / 1023.0f ) * 220.0f;
				const auto b = -110.0f + ( y / 1023.0f ) * 220.0f;

				const auto rgb = util::lab2rgb( vec3f( lightness, a, b ) );
				const auto alpha = vec2f( x / 1023.0f * 2.0f - 1.0f, y / 1023.0f * 2.0f - 1.0f ).length() / std::sqrt( 2.0f );
				pixels[x * 1024 + y] = vec4f( rgb.x, rgb.y, rgb.z, alpha );
			}
		}

		const auto context = QOpenGLContext::globalShareContext();
		const auto functions = context->functions();
		if( !_backgroundTexture ) functions->glGenTextures( 1, &_backgroundTexture );
		functions->glBindTexture( GL_TEXTURE_2D, _backgroundTexture );
		functions->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		functions->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		functions->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		functions->glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, Size, Size, 0, GL_RGBA, GL_FLOAT, pixels.data() );

		emit backgroundChanged();
	}
	float backgroundLightness() const noexcept
	{
		return _backgroundLightness;
	}

	// Getter for the volumes types
	std::pair<Ensemble::VolumeID, Ensemble::VolumeID> volumeIDs() const noexcept
	{
		return _volumeIDs;
	}

	// Setter and getter for the first domain
	void setFirstDomain( vec2d domain )
	{
		_firstDomain = domain;
		emit domainsChanged();
	}
	vec2d firstDomain() const noexcept
	{
		return _firstDomain;
	}

	// Setter and getter for the second domain
	void setSecondDomain( vec2d domain )
	{
		_secondDomain = domain;
		emit domainsChanged();
	}
	vec2d secondDomain() const noexcept
	{
		return _secondDomain;
	}

	// Getter for the polygons
	const std::vector<Polygon>& polygons() const noexcept
	{
		return _polygons;
	}
	std::vector<Polygon>& polygons() noexcept
	{
		return _polygons;
	}

	// Getter for the color map and the background texture
	GLuint texture() const noexcept
	{
		return _colorMap;
	}
	GLuint backgroundTexture() const noexcept
	{
		return _backgroundTexture;
	}

signals:
	void backgroundChanged();
	void domainsChanged();
	void colorMapChanged();
	void nameChanged( const QString& );

private:
	QString _name;
	std::pair<Ensemble::VolumeID, Ensemble::VolumeID> _volumeIDs;
	vec2d _firstDomain, _secondDomain;
	std::vector<Polygon> _polygons;
	float _backgroundLightness = std::numeric_limits<float>::infinity();

	GLuint _colorMap = 0, _backgroundTexture = 0;
};

// Class to edit a 2D color map
class ColorMap2DEditor : public QOpenGLWidget, public QOpenGLFunctions_4_5_Core
{
	Q_OBJECT
public:
	// Default constructor :)
	ColorMap2DEditor() : QOpenGLWidget(), QOpenGLFunctions_4_5_Core()
	{
		this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );
		this->setFocusPolicy( Qt::FocusPolicy::WheelFocus );
		this->setMouseTracking( true );
	}

	// Setter for the color map that is to be edited
	void setColorMap( ColorMap2D& colorMap )
	{
		if( _colorMap ) QObject::disconnect( _colorMap, nullptr, this, nullptr );
		_colorMap = &colorMap;
		QObject::connect( _colorMap, &ColorMap2D::backgroundChanged, this, &ColorMap2DEditor::updateColorMap );
		QObject::connect( _colorMap, &ColorMap2D::domainsChanged, this, &ColorMap2DEditor::updateColorMap );
		QObject::connect( _colorMap, &ColorMap2D::colorMapChanged, this, QOverload<>::of( &QWidget::update ) );

		_hoveredPolygon = _selectedPolygon = -1;
		this->update();
	}

	// Setter for the two input volumes
	void setVolumes( const Volume<float>* first, const Volume<float>* second )
	{
		_volumes = std::make_pair( first, second );

		if( first && second )
		{
			_voxelCount = first->voxelCount();

			this->makeCurrent();
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, _firstVolumeBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, first->voxelCount() * sizeof( float ), first->data(), GL_STATIC_DRAW );

			glBindBuffer( GL_SHADER_STORAGE_BUFFER, _secondVolumeBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, second->voxelCount() * sizeof( float ), second->data(), GL_STATIC_DRAW );
			this->update();
		}
	}

	// Setter for different buffers used for the scatter plot
	void setPermutationBuffer( GLuint permutationBuffer )
	{
		_permutationBuffer = permutationBuffer;
		this->update();
	}
	void setSelectionBuffer( GLuint selectionBuffer )
	{
		_selectionBuffer = selectionBuffer;
		this->update();
	}
	void setVisibilityBuffer( GLuint visibilityBuffer )
	{
		_visibilityBuffer = visibilityBuffer;
		this->update();
	}

	// Setter for the number of samples to be used in the scatter plot
	void setSampleCount( int32_t sampleCount )
	{
		_sampleCount = sampleCount;
		this->update();
	}

	// Getter for the color map that is begin edited
	ColorMap2D* colorMap() const noexcept
	{
		return _colorMap;
	}

	// Getter for the color of the currently selected polygon
	QColor color() const noexcept
	{
		const auto c = ( _selectedPolygon == -1 ? _currentColor : _colorMap->polygons()[_selectedPolygon].color ) * 255.0f;
		return QColor( c.x, c.y, c.z, c.w );
	}

signals:
	// Signals when the color of the currently selected polygon changes
	void colorChanged( QColor );

	// Signals when the color map changes in any way
	void colorMapChanged( GLuint );

	// Signals the new index of the currently selected polygon
	void selectedPolygonChanged( GLuint );

	// Signals when the selection of voxels changes
	void selectionChanged( std::vector<float> selection );

public slots:
	// Setter for the color of the currently selected polygon
	void setColor( QColor color )
	{
		const auto c = vec4f( color.red(), color.green(), color.blue(), color.alpha() ) / 255.0f;
		if( c != _currentColor )
		{
			_currentColor = c;
			if( _selectedPolygon != -1 )
			{
				_colorMap->polygons()[_selectedPolygon].color = c;
				this->updateColorMap();
			}
			emit colorChanged( this->color() );
		}
	}

	// Setter for the colors of unselected and selected samples in the scatter plot
	void setSampleColors( const QColor& unselected, const QColor& selected )
	{
		_colorSelected = vec4f( selected.red(), selected.green(), selected.blue(), selected.alpha() ) / 255.0f;
		_colorUnselected = vec4f( unselected.red(), unselected.green(), unselected.blue(), unselected.alpha() ) / 255.0f;
		this->update();
	}

	// Selects the polygon at the specified index
	void setSelectedPolygon( int32_t index )
	{
		if( index != _selectedPolygon )
		{
			emit selectedPolygonChanged( _selectedPolygon = index );
			this->update();
		}
	}

private slots:
	// Renders the 2D color map to a texture
	void updateColorMap()
	{
		this->makeCurrent();

		// Setup framebuffer
		_framebuffer->bind();
		glDrawBuffer( GL_COLOR_ATTACHMENT1 );
		glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		glClear( GL_COLOR_BUFFER_BIT );

		// Render background
		if( _colorMap->backgroundTexture() )
			glCopyImageSubData( _colorMap->backgroundTexture(), GL_TEXTURE_2D, 0, 0, 0, 0, _framebuffer->textures()[1], GL_TEXTURE_2D, 0, 0, 0, 0, ColorMap2D::Size, ColorMap2D::Size, 1 );

		// Render polygons
		for( const auto& polygon : _colorMap->polygons() )
		{
			this->paintPolygon( polygon );
			_shaderColorMap.bind();
			_shaderColorMap.setUniformValue( "color", polygon.color.x, polygon.color.y, polygon.color.z, polygon.color.w );
			glBindImageTexture( 0, _framebuffer->textures()[1], 0, false, 0, GL_READ_WRITE, GL_RGBA32F );
			glBindImageTexture( 1, _framebuffer->textures()[0], 0, false, 0, GL_READ_ONLY, GL_R16F );
			glDispatchCompute( _framebuffer->width(), _framebuffer->height(), 1 );
		}

		// Copy texture to output
		glCopyImageSubData( _framebuffer->textures()[1], GL_TEXTURE_2D, 0, 0, 0, 0, _colorMap->texture(), GL_TEXTURE_2D, 0, 0, 0, 0, ColorMap2D::Size, ColorMap2D::Size, 1 );

		_colorMap->colorMapChanged();
		this->update();
	}

private:
	// Functions that ensure that the widgets is always square
	bool hasHeightForWidth() const override
	{
		return true;
	}
	int heightForWidth( int width ) const override
	{
		return width;
	}

	// Initialize some OpenGL objects
	void initializeGL() override
	{
		if( HMODULE module = GetModuleHandleA( "renderdoc.dll" ) )
		{
			pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI) GetProcAddress( module, "RENDERDOC_GetAPI" );
			RENDERDOC_GetAPI( eRENDERDOC_API_Version_1_1_2, (void**) &_renderDoc );
		}

		QOpenGLFunctions_4_5_Core::initializeOpenGLFunctions();

		// Intialize framebuffer
		auto format = QOpenGLFramebufferObjectFormat();
		format.setInternalTextureFormat( GL_R16F );
		_framebuffer.reset( new QOpenGLFramebufferObject( ColorMap2D::Size, ColorMap2D::Size, format ) );
		_framebuffer->addColorAttachment( ColorMap2D::Size, ColorMap2D::Size, GL_RGBA32F );

		// Setup texture parameters
		glBindTexture( GL_TEXTURE_2D, _framebuffer->textures()[1] );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );

		// Compile shaders
		_shaderPoints.addShaderFromSourceCode( QOpenGLShader::Vertex,
			"#version 450\n"

			"layout( binding = 0 ) restrict readonly buffer BufferFirstValues { float firstValues[]; };"
			"layout( binding = 1 ) restrict readonly buffer BufferSecondValues { float secondValues[]; };"
			"layout( binding = 2 ) restrict readonly buffer BufferPermutation { int permutation[]; };"
			"uniform int voxelCount;"

			"uniform vec2 ranges[2];"

			"layout( location = 0 ) flat out int sampleIndex;"

			"void main() {"
			"	sampleIndex = permutation[gl_VertexID];"

			"	float x = firstValues[sampleIndex];"
			"	float y = secondValues[sampleIndex];"

			"	x = ( ( x - ranges[0].x ) / ( ranges[0].y - ranges[0].x ) * 2.0 - 1.0 ) * 0.95;"
			"	y = ( ( y - ranges[1].x ) / ( ranges[1].y - ranges[1].x ) * 2.0 - 1.0 ) * 0.95;"

			"	gl_PointSize = 2.0;"
			"	gl_Position = vec4( x, y, 0.0, 1.0 );"
			"}"
		);
		_shaderPoints.addShaderFromSourceCode( QOpenGLShader::Fragment,
			"#version 450\n"

			"layout( binding = 3 ) restrict readonly buffer BufferVisibility { int visibility[]; };"
			"layout( binding = 4 ) restrict readonly buffer BufferSelection { float selection[]; };"
			"layout( location = 0 ) flat in int sampleIndex;"
			"uniform vec4 colorSelected;"
			"uniform vec4 colorUnselected;"

			"layout( location = 0 ) out vec4 outColor;"

			"void main() {"
			"	if( false && visibility[sampleIndex] == 0 ) discard;"
			"	outColor = selection[sampleIndex] != 0? colorSelected : colorUnselected;"
			"}"
		);
		_shaderPoints.link();

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

		_shaderBlend.addShaderFromSourceCode( QOpenGLShader::Vertex,
			"#version 450\n"

			"layout( location = 0 ) out vec2 outTextureCoords;"

			"void main() {"
			"	const vec2 positions[4]		= const vec2[4]( vec2( -1.0, 1.0 ), vec2( -1.0, -1.0 ), vec2( 1.0, 1.0 ), vec2( 1.0, -1.0 ) );"
			"	const vec2 textureCoords[4] = const vec2[4]( vec2( 0.0, 1.0 ), vec2( 0.0, 0.0 ), vec2( 1.0, 1.0 ), vec2( 1.0, 0.0 ) );"

			"	outTextureCoords = textureCoords[gl_VertexID];"
			"	gl_Position = vec4( positions[gl_VertexID], 0.0, 1.0 );"
			"}"
		);
		_shaderBlend.addShaderFromSourceCode( QOpenGLShader::Fragment,
			"#version 450\n"

			"layout( location = 0 ) in vec2 inTextureCoords;"
			"uniform sampler2D inTexture;"
			"uniform vec4 color;"
			"uniform bool useColor;"

			"layout( location = 0 ) out vec4 outColor;"

			"void main() {"
			"	vec4 texel = texture( inTexture, inTextureCoords );"
			"	outColor = useColor? ( texel.x == 0.0? vec4( 0.0 ) : color ) : texel;"
			"}"
		);
		_shaderBlend.link();

		_shaderColorMap.addShaderFromSourceCode( QOpenGLShader::Compute,
			"#version 450\n"

			"layout( local_size_x = 1 ) in;"
			"layout( binding = 0, rgba32f ) uniform restrict image2D inoutColorMap;"
			"layout( binding = 1, r16f ) uniform restrict readonly image2D inPolygon;"
			"uniform vec4 color;"

			"void main() {"
			"	const ivec2 texel = ivec2( gl_WorkGroupID.xy );"

			"	vec4 colorMap = imageLoad( inoutColorMap, texel );"
			"	vec4 polygon = imageLoad( inPolygon, texel ).x == 0.0? vec4( 0.0 ) : color;"

			"	vec3 rgb = polygon.rgb * polygon.a * colorMap.a + polygon.rgb * ( 1.0 - colorMap.a ) + colorMap.rgb * ( 1.0 - polygon.a );"
			"	float a = polygon.a * colorMap.a + polygon.a * ( 1.0 - colorMap.a ) + colorMap.a * ( 1.0 - polygon.a );"
			"	vec4 color = vec4( rgb, a );"

			"	imageStore( inoutColorMap, texel, color );"
			"}"
		);
		_shaderColorMap.link();

		_shaderSelection.addShaderFromSourceCode( QOpenGLShader::Compute,
			"#version 450\n"

			"layout( local_size_x = 1 ) in;"
			"layout( binding = 0 ) restrict readonly buffer BufferFirstValues { float firstValues[]; };"
			"layout( binding = 1 ) restrict readonly buffer BufferSecondValues { float secondValues[]; };"
			"layout( binding = 2 ) restrict buffer BufferSelection { float selection[]; };"
			"uniform sampler2D inTexture;"
			"uniform vec2 ranges[2];"
			"uniform int mode;"

			"void main() {"
			"	int sampleIndex = int( gl_GlobalInvocationID.x );"

			"	float x = firstValues[sampleIndex];"
			"	float y = secondValues[sampleIndex];"

			"	x = ( ( x - ranges[0].x ) / ( ranges[0].y - ranges[0].x ) * 2.0 - 1.0 ) * 0.95;"
			"	y = ( ( y - ranges[1].x ) / ( ranges[1].y - ranges[1].x ) * 2.0 - 1.0 ) * 0.95;"

			"	x = ( x + 1.0 ) / 2.0;"
			"	y = ( y + 1.0 ) / 2.0;"

			"	if( mode == 0 && selection[sampleIndex] == 0.0f && ( texture( inTexture, vec2( x, y ) ).x != 0.0 ) )"
			"		selection[sampleIndex] = 1.0f;"

			"	if( mode == 1 && selection[sampleIndex] == 1.0f && ( texture( inTexture, vec2( x, y ) ).x != 0.0 ) )"
			"		selection[sampleIndex] = 0.0f;"
			"}"
		);
		_shaderSelection.link();

		// Create buffers for the input volumes and the polygon points
		glGenBuffers( 1, &_firstVolumeBuffer );
		glGenBuffers( 1, &_secondVolumeBuffer );
		glGenBuffers( 1, &_currentPolygon.buffer );
	}

	// Render the scatter plot, polygons etc.
	void paintGL() override
	{
		if( !_colorMap ) return;
		if( _captureFrame && _renderDoc ) _renderDoc->StartFrameCapture( nullptr, nullptr );

		// Clear background
		glClearColor( 0.975f, 0.975f, 0.975f, 1.0f );
		glClear( GL_COLOR_BUFFER_BIT );

		// Render background
		if( _colorMap->backgroundTexture() )
		{
			_shaderBlend.bind();
			_shaderBlend.setUniformValue( "useColor", false );
			_shaderBlend.setUniformValue( "inTexture", 0 );

			// glEnable( GL_BLEND );
			// glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			glBindTexture( GL_TEXTURE_2D, _colorMap->backgroundTexture() );
			glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
			// glDisable( GL_BLEND );
		}

		// Render scatter plot
		if( _volumes.first && _volumes.second )
		{
			_shaderPoints.bind();
			_shaderPoints.setUniformValue( "voxelCount", _volumes.first->voxelCount() );
			_shaderPoints.setUniformValue( "ranges[0]", _colorMap->firstDomain().x, _colorMap->firstDomain().y );
			_shaderPoints.setUniformValue( "ranges[1]", _colorMap->secondDomain().x, _colorMap->secondDomain().y );
			_shaderPoints.setUniformValue( "colorSelected", _colorSelected.x, _colorSelected.y, _colorSelected.z, _colorSelected.w );
			_shaderPoints.setUniformValue( "colorUnselected", _colorUnselected.x, _colorUnselected.y, _colorUnselected.z, _colorUnselected.w );

			glEnable( GL_VERTEX_PROGRAM_POINT_SIZE );
			glEnable( GL_BLEND );
			glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, _firstVolumeBuffer );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, _secondVolumeBuffer );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, _permutationBuffer );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, _visibilityBuffer );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 4, _selectionBuffer );
			glDrawArrays( GL_POINTS, 0, _sampleCount );

			glDisable( GL_VERTEX_PROGRAM_POINT_SIZE );
			glDisable( GL_BLEND );
		}

		// Draw color map
		for( const auto& polygon : _colorMap->polygons() )
		{
			this->paintPolygon( polygon );

			this->makeCurrent();
			glViewport( 0, 0, width(), height() );
			this->blendPolygon( polygon, 0.1f );
		}

		// Draw current polygon
		if( _currentPolygon.points.size() )
		{
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, _currentPolygon.buffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, _currentPolygon.points.size() * sizeof( vec2f ), _currentPolygon.points.data(), GL_STATIC_DRAW );
			this->paintPolygon( _currentPolygon );

			this->makeCurrent();
			glViewport( 0, 0, width(), height() );
			this->blendPolygon( _currentPolygon, 0.1f );
		}

		// Draw hovered polygon border
		if( _hoveredPolygon != -1 )
		{
			const auto& polygon = _colorMap->polygons()[_hoveredPolygon];
			_shaderPolygon.bind();
			_shaderPolygon.setUniformValue( "color", 0.6f, 0.6f, 0.6f, 1.0f );

			glLineWidth( 2.0f );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, polygon.buffer );
			glDrawArrays( GL_LINE_LOOP, 0, polygon.points.size() );
		}

		// Draw selected polygon border
		if( _selectedPolygon != -1 )
		{
			const auto& polygon = _colorMap->polygons()[_selectedPolygon];
			const auto gray = _selectedPolygon == _hoveredPolygon ? 0.4f : 0.1f;
			_shaderPolygon.bind();
			_shaderPolygon.setUniformValue( "color", gray, gray, gray, 1.0f );

			glLineWidth( 2.0f );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, polygon.buffer );
			glDrawArrays( GL_LINE_LOOP, 0, polygon.points.size() );
		}

		if( _captureFrame && _renderDoc ) _renderDoc->EndFrameCapture( nullptr, nullptr ), _captureFrame = false;
	}

	// Renders a polygon to the first color attachment of the frame buffer; resulting in a binary texture
	void paintPolygon( const ColorMap2D::Polygon& polygon )
	{
		this->makeCurrent();

		_framebuffer->bind();
		glDrawBuffer( GL_COLOR_ATTACHMENT0 );
		glViewport( 0, 0, _framebuffer->width(), _framebuffer->height() );
		glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
		glClear( GL_COLOR_BUFFER_BIT );

		_shaderPolygon.bind();
		_shaderPolygon.setUniformValue( "color", 1.0f, 1.0f, 1.0f, 1.0f );

		glEnable( GL_BLEND );
		glBlendFunc( GL_ONE_MINUS_DST_COLOR, GL_ZERO );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, polygon.buffer );
		glDrawArrays( GL_TRIANGLE_FAN, 0, polygon.points.size() );
		glDisable( GL_BLEND );
	}

	// Renders a polygon with blending, using the mask in the first framebuffer attachment
	void blendPolygon( const ColorMap2D::Polygon& polygon, float minAlpha )
	{
		_shaderBlend.bind();
		_shaderBlend.setUniformValue( "color", polygon.color.x, polygon.color.y, polygon.color.z, std::max( minAlpha, polygon.color.w ) );
		_shaderBlend.setUniformValue( "useColor", true );
		_shaderBlend.setUniformValue( "inTexture", 0 );

		glEnable( GL_BLEND );
		glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE );
		glBindTexture( GL_TEXTURE_2D, _framebuffer->textures()[0] );
		glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
		glDisable( GL_BLEND );
	}

	// Handles keyboard events
	void keyPressEvent( QKeyEvent* event ) override
	{
		// Set flag to capture the next frame
		if( event->key() == Qt::Key_C )
		{
			_captureFrame = true;
			this->update();
		}
	}

	// Handles mouse move events
	void mouseMoveEvent( QMouseEvent* event ) override
	{
		if( event->buttons() == Qt::NoButton ) this->updateHoveredPolygon( event->pos() );
		else if( event->modifiers() & Qt::ShiftModifier || event->buttons() == Qt::LeftButton )
		{
			_currentPolygon.screenPoints.push_back( event->pos() );
			_currentPolygon.points.push_back( this->screenToPoint( event->pos() ) );
			this->update();
		}
	}

	// Handles mouse button events
	void mousePressEvent( QMouseEvent* event ) override
	{
		if( event->modifiers() & Qt::ShiftModifier )
		{
			_currentPolygon.screenPoints.push_back( event->pos() );
			_currentPolygon.points.push_back( this->screenToPoint( event->pos() ) );
			_currentPolygon.color = _colorSelected;
		}
		else
		{
			if( event->button() == Qt::LeftButton )
			{
				_currentPolygon.screenPoints.push_back( event->pos() );
				_currentPolygon.points.push_back( this->screenToPoint( event->pos() ) );
				_currentPolygon.color = _currentColor;
			}
			else if( event->button() == Qt::RightButton )
			{
				// Delete the hovered polygon with a right click
				if( _hoveredPolygon != -1 )
				{
					glDeleteBuffers( 1, &_colorMap->polygons()[_hoveredPolygon].buffer );
					_colorMap->polygons().erase( _colorMap->polygons().begin() + _hoveredPolygon );

					this->updateHoveredPolygon( event->pos() );
					this->updateColorMap();
				}
			}
		}
	}
	void mouseReleaseEvent( QMouseEvent* event ) override
	{
		if( event->modifiers() & Qt::ShiftModifier )
		{
			// Change the selection of voxels
			if( _volumes.first && _volumes.second )
			{
				this->makeCurrent();
				glBindBuffer( GL_SHADER_STORAGE_BUFFER, _currentPolygon.buffer );
				glBufferData( GL_SHADER_STORAGE_BUFFER, _currentPolygon.points.size() * sizeof( vec2f ), _currentPolygon.points.data(), GL_STATIC_DRAW );

				this->paintPolygon( _currentPolygon );

				_shaderSelection.bind();
				_shaderSelection.setUniformValue( "ranges[0]", _colorMap->firstDomain().x, _colorMap->firstDomain().y );
				_shaderSelection.setUniformValue( "ranges[1]", _colorMap->secondDomain().x, _colorMap->secondDomain().y );
				_shaderSelection.setUniformValue( "mode", ( event->button() == Qt::LeftButton ? 0 : 1 ) );
				_shaderSelection.setUniformValue( "inTexture", 0 );

				glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, _firstVolumeBuffer );
				glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, _secondVolumeBuffer );
				glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, _selectionBuffer );
				glBindTexture( GL_TEXTURE_2D, _framebuffer->textures()[0] );
				glDispatchCompute( _voxelCount, 1, 1 );

				auto values = std::vector<float>( _voxelCount );
				glBindBuffer( GL_SHADER_STORAGE_BUFFER, _selectionBuffer );
				glGetBufferSubData( GL_SHADER_STORAGE_BUFFER, 0, _voxelCount * sizeof( int32_t ), values.data() );
				emit selectionChanged( std::move( values ) );
			}

			_currentPolygon.screenPoints.clear();
			_currentPolygon.points.clear();
			this->update();
		}
		else if( event->button() == Qt::LeftButton )
		{
			if( _hoveredPolygon != -1 && _currentPolygon.points.size() == 1 )
			{
				// Select clicked polygon
				this->setSelectedPolygon( ( _hoveredPolygon == _selectedPolygon ) ? -1 : _hoveredPolygon );
				emit colorChanged( this->color() );
			}
			else
			{
				// Add a new polygon
				this->makeCurrent();
				glBindBuffer( GL_SHADER_STORAGE_BUFFER, _currentPolygon.buffer );
				glBufferData( GL_SHADER_STORAGE_BUFFER, _currentPolygon.points.size() * sizeof( vec2f ), _currentPolygon.points.data(), GL_STATIC_DRAW );
				_colorMap->polygons().push_back( _currentPolygon );

				glGenBuffers( 1, &_currentPolygon.buffer );

				this->updateColorMap();
				this->updateHoveredPolygon( event->pos() );
			}

			_currentPolygon.screenPoints.clear();
			_currentPolygon.points.clear();
			this->update();
		}
	}

	// Reset the hovered polygon when the mouse leaves
	void leaveEvent( QEvent* event ) override
	{
		_hoveredPolygon = -1;
		this->update();
	}

	// Updates the hovered polygon
	void updateHoveredPolygon( QPoint cursor )
	{
		struct { int32_t index; double distance; } closest { -1, std::numeric_limits<double>::max() };

		for( int32_t i = 0; i < _colorMap->polygons().size(); ++i )
		{
			if( !_colorMap->polygons()[i].screenPoints.containsPoint( cursor, Qt::OddEvenFill ) ) continue;

			const auto distance = QLineF( cursor, _colorMap->polygons()[i].screenPoints.boundingRect().center() ).length();
			if( distance < closest.distance ) closest = { i, distance };
		}

		if( closest.index != _hoveredPolygon )
		{
			_hoveredPolygon = closest.index;
			this->update();
		}
	}

	// Helper function to map a screen point to the scatter plot
	vec2f screenToPoint( QPoint point )
	{
		const auto x = static_cast<double>( point.x() ) / this->width() * 2.0 - 1.0;
		const auto y = static_cast<double>( point.y() ) / this->height() * -2.0 + 1.0;

		return vec2f( x, y );
	}

	RENDERDOC_API_1_4_1* _renderDoc = nullptr;
	bool _captureFrame = false;

	ColorMap2D* _colorMap = nullptr;
	std::pair<const Volume<float>*, const Volume<float>*> _volumes;

	std::unique_ptr<QOpenGLFramebufferObject> _framebuffer;
	QOpenGLShaderProgram _shaderPoints, _shaderPolygon, _shaderBlend, _shaderColorMap, _shaderSelection;

	GLuint _firstVolumeBuffer = 0, _secondVolumeBuffer = 0, _permutationBuffer = 0, _selectionBuffer = 0, _visibilityBuffer = 0;
	int32_t _voxelCount = 0, _sampleCount = 0;

	vec4f _colorSelected, _colorUnselected;
	vec4f _currentColor = vec4f( 1.0f, 0.0f, 0.0f, 0.1f );

	ColorMap2D::Polygon _currentPolygon;
	int32_t _hoveredPolygon = -1, _selectedPolygon = -1;
};



// Class to manage color maps
class ColorMapManager : public QWidget
{
	Q_OBJECT
public:
	ColorMapManager() : QWidget()
	{
		this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );

		_layout = new QFormLayout( this );
		_layout->setContentsMargins( 10, 10, 10, 10 );
		_layout->setSpacing( 10 );
		_layout->setAlignment( Qt::AlignTop );

		_colorMapSelector = new ItemList<QString>( "Color Map" );
		_colorMapSelector->addItem( "Color Map", "" );

		_colorMap1DEditor = new ColorMap1DEditor;
		_colorMap2DEditor = new ColorMap2DEditor;
	}

	// Getter for 1D and 2D color maps
	const std::vector<ColorMap1D*>& colorMaps1D( Ensemble::VolumeID type )
	{
		auto& colorMaps = _colorMaps1D[type];
		if( colorMaps.empty() ) this->addColorMap( type, Ensemble::VolumeID( -1, Ensemble::Derived::eNone ) );
		return colorMaps;
	}
	const std::vector<ColorMap2D*>& colorMaps2D( Ensemble::VolumeID first, Ensemble::VolumeID second )
	{
		if( first > second ) std::swap( first, second );

		auto& colorMaps = _colorMaps2D[first][second];
		if( colorMaps.empty() ) this->addColorMap( first, second );
		return colorMaps;
	}

	// Return the currently edited 1D or 2D color map
	const ColorMap1D* currentColorMap1D() const noexcept
	{
		return _colorMap1DEditor->isVisible() ? _colorMap1DEditor->colorMap() : nullptr;
	}
	const ColorMap2D* currentColorMap2D() const noexcept
	{
		return _colorMap2DEditor->isVisible() ? _colorMap2DEditor->colorMap() : nullptr;
	}

signals:
	// Signals when the color of a node (1D color map) or polygon (2D color map) changes
	void colorChanged( QColor );

	// Signals when a 1D color map is removed or added
	void colorMap1DAdded( Ensemble::VolumeID, ColorMap1D* );
	void colorMap1DRemoved( Ensemble::VolumeID, ColorMap1D* );

	// Signals when a 2D color map is removed or added
	void colorMap2DAdded( Ensemble::VolumeID, Ensemble::VolumeID, ColorMap2D* );
	void colorMap2DRemoved( Ensemble::VolumeID, Ensemble::VolumeID, ColorMap2D* );

public slots:
	// Setter for the color of the current node (1D color map) or polygon (2D color map)
	void setColor( QColor color )
	{
		_colorMap1DEditor->setColor( color );
		_colorMap2DEditor->setColor( color );
	}

	// Setter for the color of samples in the scatter plot of the 2D color map editor
	void setSampleColors( const QColor& unselected, const QColor& selected )
	{
		_colorMap2DEditor->setSampleColors( unselected, selected );
	}

	// Setter for the currently used ensembles
	void setEnsembles( const Ensemble& ensemble, const Ensemble* differenceEnsemble )
	{
		const auto firstTime = !_ensemble;
		_ensemble = &ensemble;
		_differenceEnsemble = differenceEnsemble;

		if( firstTime ) this->initialize();
		else this->selectColorMap( _colorMapSelector->index() );
	}

	// Setter for the permutation buffer (permutation of sample indices) and the sample count that should be used
	void setPermutationBuffer( GLint buffer )
	{
		if( _colorMap2DEditor ) _colorMap2DEditor->setPermutationBuffer( buffer );
	}
	void setSampleCount( int32_t sampleCount )
	{
		if( _colorMap2DEditor ) _colorMap2DEditor->setSampleCount( sampleCount );
	}

	// Setter for the current region, which determines the selected samples
	void setRegion( Region* region )
	{
		if( _region != region )
		{
			if( _region ) QObject::disconnect( _region, nullptr, this, nullptr );
			_region = region;

			if( _region )
			{
				QObject::connect( _region, &Region::selectionChanged, this, &ColorMapManager::onRegionChanged );
				this->onRegionChanged();
			}
		}
	}

private slots:
	// Initialize widgets and their layout
	void initialize()
	{
		// Layout widgets
		_layout->addRow( "First Volume", _firstVolume = new VolumePicker( *_ensemble, false, true ) );
		_layout->addRow( "Second Volume", _secondVolume = new VolumePicker( *_ensemble, true, true ) );

		_layout->addRow( "Name", _colorMapSelector );
		_layout->addRow( _colorMap1DEditor );
		_layout->addRow( _colorMap2DEditor );

		_firstDomain = new RangeWidget();
		_secondDomain = new RangeWidget();
		_fitFirstDomain = new QPushButton( "Fit" );
		_fitSecondDomain = new QPushButton( "Fit" );
		_lightness = new NumberWidget( 0, 100, 50 );

		_colorMap2DEditor->setVisible( false );

		// Add some color maps by default
		for( int32_t field = 0; field < _ensemble->fieldCount(); ++field )
		{
			for( const auto type : VolumePicker::types() )
			{
				this->addColorMap( Ensemble::VolumeID( field, type, false ), Ensemble::VolumeID( field, Ensemble::Derived::eNone ) );
				this->addColorMap( Ensemble::VolumeID( field, type, true ), Ensemble::VolumeID( field, Ensemble::Derived::eNone ) );
			}
		}

		// Initialize connections
		QObject::connect( _firstVolume, &VolumePicker::volumeIDChanged, this, &ColorMapManager::onVolumeChanged );
		QObject::connect( _secondVolume, &VolumePicker::volumeIDChanged, this, &ColorMapManager::onVolumeChanged );
		QObject::connect( _colorMapSelector, &ItemListSignals::itemNameChanged, this, QOverload<int32_t, const QString&>::of( &ColorMapManager::setColorMapName ) );
		QObject::connect( _colorMapSelector, &ItemListSignals::indexChanged, this, &ColorMapManager::selectColorMap );
		QObject::connect( _colorMapSelector, &ItemListSignals::itemAdded, this, QOverload<>::of( &ColorMapManager::addColorMap ) );
		QObject::connect( _colorMapSelector, &ItemListSignals::itemRemoved, this, &ColorMapManager::removeColorMap );
		QObject::connect( _colorMap1DEditor, &ColorMap1DEditor::colorChanged, this, &ColorMapManager::colorChanged );
		QObject::connect( _colorMap2DEditor, &ColorMap2DEditor::colorChanged, this, &ColorMapManager::colorChanged );
		QObject::connect( _colorMap2DEditor, &ColorMap2DEditor::selectionChanged, this, &ColorMapManager::onSelectionChanged );

		QObject::connect( _firstDomain, &RangeWidget::valuesChanged, [=] ( double lower, double upper ) { _colorMap2DEditor->colorMap()->setFirstDomain( vec2d( lower, upper ) ); } );
		QObject::connect( _secondDomain, &RangeWidget::valuesChanged, [=] ( double lower, double upper ) { _colorMap2DEditor->colorMap()->setSecondDomain( vec2d( lower, upper ) ); } );
		QObject::connect( _fitFirstDomain, &QPushButton::clicked, [=]
		{
			auto type = _firstVolume->volumeID();
			const auto& volume = ( type.difference && _differenceEnsemble ) ? _ensemble->differenceVolume( type, *_differenceEnsemble ) : _ensemble->volume( type );
			_firstDomain->setValues( volume.domain().x, volume.domain().y );
		} );
		QObject::connect( _fitSecondDomain, &QPushButton::clicked, [=]
		{
			auto type = _secondVolume->volumeID();
			const auto& volume = ( type.difference && _differenceEnsemble ) ? _ensemble->differenceVolume( type, *_differenceEnsemble ) : _ensemble->volume( type );
			_secondDomain->setValues( volume.domain().x, volume.domain().y );
		} );
		QObject::connect( _lightness, &NumberWidget::valueChanged, [=] ( double lightness ) { _colorMap2DEditor->colorMap()->setBackgrond( lightness ); } );
	}

	// Event handler for when an input volume changes; updates the current list of color maps
	void onVolumeChanged()
	{
		auto first = _firstVolume->volumeID();
		auto second = _secondVolume->volumeID();

		_colorMapSelector->blockSignals( true );
		while( _colorMapSelector->itemCount() ) _colorMapSelector->removeItem( 0 );

		if( second.type == Ensemble::Derived::eNone )
		{
			auto& colorMaps = _colorMaps1D[first];
			if( colorMaps.empty() ) this->addColorMap();
			for( auto colorMap : colorMaps ) _colorMapSelector->addItem( colorMap->name(), "" );
		}
		else
		{
			if( first > second ) std::swap( first, second );

			auto& colorMaps = _colorMaps2D[first][second];
			if( colorMaps.empty() ) this->addColorMap();
			for( auto colorMap : colorMaps ) _colorMapSelector->addItem( colorMap->name(), "" );
		}

		_colorMapSelector->blockSignals( false );
		emit _colorMapSelector->indexChanged( _colorMapSelector->index() );
	}

	// Event handler for when the current region changes; notifies the color map editors
	void onRegionChanged()
	{
		if( _ensemble ) _colorMap1DEditor->setMask( _region->createMask( *_ensemble ) );
		_colorMap2DEditor->setSelectionBuffer( _region->selectionBuffer() );
	}

	// Event handler for when the selection of samples in the 2D color map editor changes; updates the current region
	void onSelectionChanged( std::vector<float> selection )
	{
		if( _ensemble && _region )
			_region->setConstantMask( std::make_shared<Volume<float>>( _ensemble->dimensions(), std::move( selection ), "" ) );
	}

	// Setters for the name of a color map
	void setColorMapName( Ensemble::VolumeID first, Ensemble::VolumeID second, int32_t index, const QString& name )
	{
		if( second.type == Ensemble::Derived::eNone )
		{
			_colorMaps1D[first][index]->setName( name );
		}
		else
		{
			if( first > second ) std::swap( first, second );
			_colorMaps2D[first][second][index]->setName( name );
		}
	}
	void setColorMapName( int32_t index, const QString& name )
	{
		auto first = _firstVolume->volumeID();
		auto second = _secondVolume->volumeID();
		this->setColorMapName( first, second, index, name );
	}

	// Function to add and remove color maps
	void addColorMap( Ensemble::VolumeID first, Ensemble::VolumeID second )
	{
		if( second.type == Ensemble::Derived::eNone )
		{
			if( auto it = _domains.find( first ); it == _domains.end() )
			{
				auto& domain = _domains[first] = _ensemble->volume( first ).domain();
				if( first.difference )
				{
					const auto diff = 0.05 * ( domain.y - domain.x );
					domain = vec2d( -diff, diff );
				}
			}

			auto current = _colorMap1DEditor->colorMap();
			auto colorMap = ( current && current->volumeID() == first ) ? new ColorMap1D( "Color Map", *_colorMap1DEditor->colorMap() )
				: new ColorMap1D( "Color Map", first, _domains[first], first.difference );

			auto& colorMaps = _colorMaps1D[first];
			colorMaps.push_back( colorMap );
			this->selectColorMap( colorMaps.size() - 1 );

			emit colorMap1DAdded( first, colorMap );
		}
		else
		{
			if( first > second ) std::swap( first, second );

			if( auto it = _domains.find( first ); it == _domains.end() )
			{
				auto& domain = _domains[first] = _ensemble->volume( first ).domain();
				if( first.difference )
				{
					const auto diff = 0.05 * ( domain.y - domain.x );
					domain = vec2d( -diff, diff );
				}
			}

			if( auto it = _domains.find( second ); it == _domains.end() )
			{
				auto& domain = _domains[second] = _ensemble->volume( second ).domain();
				if( first.difference )
				{
					const auto diff = 0.05 * ( domain.y - domain.x );
					domain = vec2d( -diff, diff );
				}
			}

			auto current = _colorMap2DEditor->colorMap();
			auto colorMap = ( current && current->volumeIDs() == std::make_pair( first, second ) ) ? new ColorMap2D( "Color Map", *current )
				: new ColorMap2D( "Color Map", first, second, _domains[first], _domains[second] );

			auto& colorMaps = _colorMaps2D[first][second];
			colorMaps.push_back( colorMap );
			this->selectColorMap( colorMaps.size() - 1 );

			emit colorMap2DAdded( first, second, colorMap );
		}
	}
	void addColorMap()
	{
		auto first = _firstVolume->volumeID();
		auto second = _secondVolume->volumeID();
		this->addColorMap( first, second );
	}
	void removeColorMap( int32_t index )
	{
		auto first = _firstVolume->volumeID();
		auto second = _secondVolume->volumeID();

		if( second.type == Ensemble::Derived::eNone )
		{
			auto& colorMaps = _colorMaps1D[first];
			auto colorMap = colorMaps[index];
			colorMap->deleteLater();
			colorMaps.erase( colorMaps.begin() + index );

			emit colorMap1DRemoved( first, colorMap );
		}
		else
		{
			if( first > second ) std::swap( first, second );
			auto& colorMaps = _colorMaps2D[first][second];
			auto colorMap = colorMaps[index];
			colorMap->deleteLater();
			colorMaps.erase( colorMaps.begin() + index );

			emit colorMap2DRemoved( first, second, colorMap );
		}
	}

	// Function to select a color map; changes the layout to swap 1D and 2D color map editors and other controls
	void selectColorMap( int32_t index )
	{
		auto first = _firstVolume->volumeID();
		auto second = _secondVolume->volumeID();

		_colorMap1DEditor->setVisible( second.type == Ensemble::Derived::eNone );
		_colorMap2DEditor->setVisible( second.type != Ensemble::Derived::eNone );

		if( second.type == Ensemble::Derived::eNone )
		{
			_colorMap2DEditor->setSelectedPolygon( -1 );
			auto colorMap = _colorMaps1D[first][index];

			// Show color map preview
			if( colorMap != _currentColorMap1D )
			{
				this->hideRow( _currentColorMap1D );
				this->insertRow( "", _currentColorMap1D = colorMap, _colorMapSelector );
			}

			// Hide controls
			this->hideRow( _firstDomain );
			this->hideRow( _fitFirstDomain );
			this->hideRow( _secondDomain );
			this->hideRow( _fitSecondDomain );
			this->hideRow( _lightness );

			_colorMap1DEditor->setColorMap( *colorMap );

			const auto volume = this->volumeFromType( first );
			if( volume ) this->updateDomain( first, volume->domain() );
			_colorMap1DEditor->setVolume( volume );
		}
		else
		{
			if( first > second ) std::swap( first, second );
			auto colorMap = _colorMaps2D[first][second][index];
			_colorMap2DEditor->setColorMap( *colorMap );

			// Update volumes
			const auto firstVolume = this->volumeFromType( first );
			const auto secondVolume = this->volumeFromType( second );

			if( firstVolume ) this->updateDomain( first, firstVolume->domain() );
			if( secondVolume ) this->updateDomain( second, secondVolume->domain() );
			_colorMap2DEditor->setVolumes( firstVolume, secondVolume );

			// Hide 1D color map
			_colorMap1DEditor->deselectNode();
			this->hideRow( _currentColorMap1D );

			// Hide lightness control
			this->hideRow( _lightness );

			// Show domain controls
			if( _layout->indexOf( _firstDomain ) == -1 )
			{
				this->insertRow( "Domain (x)", _firstDomain, _colorMap2DEditor );
				this->insertRow( " ", _fitFirstDomain, _firstDomain );
				this->insertRow( "Domain (y)", _secondDomain, _fitFirstDomain );
				this->insertRow( " ", _fitSecondDomain, _secondDomain );
			}

			// Update domain controls
			_firstDomain->blockSignals( true );
			_firstDomain->setRange( _domains[first].x, _domains[first].y, true );
			_firstDomain->blockSignals( false );
			_firstDomain->setValues( colorMap->firstDomain().x, colorMap->firstDomain().y );

			_secondDomain->blockSignals( true );
			_secondDomain->setRange( _domains[second].x, _domains[second].y, true );
			_secondDomain->blockSignals( false );
			_secondDomain->setValues( colorMap->secondDomain().x, colorMap->secondDomain().y );

			// Show lightness control
			if( _layout->indexOf( _lightness ) == -1 && first.difference && second.difference )
				this->insertRow( "Lightness", _lightness, _secondDomain, 2 );
		}
	}

private:
	// Function that updates the domain of a specified volume type
	void updateDomain( const Ensemble::VolumeID& type, vec2d domain )
	{
		domain.x = std::min( domain.x, _domains[type].x );
		domain.y = std::max( domain.y, _domains[type].y );

		// For difference types, center the domain at 0
		if( type.difference )
		{
			const auto abs = std::max( std::abs( domain.x ), std::abs( domain.y ) );
			domain = vec2d( -abs, abs );
		}

		if( domain != _domains[type] )
			_domains[type] = domain;
	}

	// Helper functions for when the layout needs to change due to a change in color map
	void hideRow( QWidget* widget )
	{
		if( !widget ) return;

		auto label = _layout->labelForField( widget );
		_layout->removeWidget( label );
		if( label ) label->deleteLater();

		_layout->removeWidget( widget );
		widget->setVisible( false );
	}
	void insertRow( const QString& text, QWidget* widget, const QWidget* find, int32_t offset = 1 )
	{
		if( _layout->indexOf( widget ) != -1 ) return;

		for( int32_t i = 0; i < _layout->rowCount(); ++i )
		{
			auto item = _layout->itemAt( i, QFormLayout::ItemRole::FieldRole );
			if( item && find == item->widget() )
			{
				if( text.isEmpty() ) _layout->insertRow( i + offset, widget );
				else _layout->insertRow( i + offset, text, widget );
				widget->setVisible( true );

				return;
			}
		}
	}

	// Return the relevant volume for a specified type
	const Volume<float>* volumeFromType( const Ensemble::VolumeID& type )
	{
		if( type.difference )
		{
			if( _differenceEnsemble ) return &_ensemble->differenceVolume( type, *_differenceEnsemble );
			else return nullptr;
		}
		else return &_ensemble->volume( type );
	}

	const Ensemble* _ensemble = nullptr;
	const Ensemble* _differenceEnsemble = nullptr;
	Region* _region = nullptr;

	QFormLayout* _layout = nullptr;
	VolumePicker* _firstVolume = nullptr;
	VolumePicker* _secondVolume = nullptr;
	ItemList<QString>* _colorMapSelector = nullptr;

	std::unordered_map<Ensemble::VolumeID, vec2d> _domains;

	std::unordered_map<Ensemble::VolumeID, std::vector<ColorMap1D*>> _colorMaps1D;
	std::unordered_map<Ensemble::VolumeID, std::unordered_map<Ensemble::VolumeID, std::vector<ColorMap2D*>>> _colorMaps2D;
	ColorMap1D* _currentColorMap1D = nullptr;

	ColorMap1DEditor* _colorMap1DEditor = nullptr;
	ColorMap2DEditor* _colorMap2DEditor = nullptr;
	QWidget* _currentColorMapEditor = nullptr;

	RangeWidget* _firstDomain = nullptr;
	RangeWidget* _secondDomain = nullptr;
	QPushButton* _fitFirstDomain = nullptr;
	QPushButton* _fitSecondDomain = nullptr;
	NumberWidget* _lightness = nullptr;
};