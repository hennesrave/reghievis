#pragma once
#include <qevent.h>
#include <qpainter.h>
#include <qpropertyanimation.h>
#include <qwidget.h>

#include <unordered_set>

#include "ensemble.hpp"

// Class to manage the dendrogram visualization
class Dendrogram : public QWidget
{
	Q_OBJECT
		Q_PROPERTY( float interpolation MEMBER _interpolation );
public:
	enum class Visualization : int32_t { eComplete, eCompressed };

	Dendrogram() : QWidget(), _animation( new QPropertyAnimation( this, "interpolation" ) )
	{
		this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Minimum );
		this->setMouseTracking( true );

		// Initalize animation
		_animation->setStartValue( 0.0f );
		_animation->setEndValue( 1.0f );
		_animation->setDuration( 1000 );

		QObject::connect( _animation, &QPropertyAnimation::valueChanged, [this] { this->update(); } );
	}

	// Getter for the used similarity measure
	Ensemble::SimilarityID similarityID() const noexcept
	{
		return _similarityID;
	}

	// Getter for the visualization
	Visualization visualization() const noexcept
	{
		return _visualization;
	}

	// Getter for different nodes
	const HCNode* root() const noexcept
	{
		return _root;
	}
	const HCNode* hoveredNode() const noexcept
	{
		return _hoveredNode;
	}
	const HCNode* selectedNode() const noexcept
	{
		return _selectedNode;
	}
	const std::unordered_set<const HCNode*>& highlightedNodes() const noexcept
	{
		return _highlightedNodes;
	}

signals:
	// Signals when the used similarity measure changes
	void similarityIDChanged( const Ensemble::SimilarityID& );

	// Signals when the visualization changes
	void visualizationChanged( Visualization );

	// Signals when the option to use the similarity for the vertical position of nodes changes
	void similarityForHeightChanged( bool );

	// Signals when different nodes change
	void rootChanged( const HCNode* );
	void hoveredNodeChanged( const HCNode* );
	void selectedNodeChanged( const HCNode* );
	void highlightedNodesChanged( const std::unordered_set<const HCNode*>& );

public slots:
	// Setter for the similarity measure used for clustering
	void setSimilarity( const Ensemble::SimilarityID& similarityID )
	{
		if( similarityID != _similarityID ) emit similarityIDChanged( _similarityID = similarityID );
	}

	// Setters for the visualization method
	void setVisualization( Visualization visualization )
	{
		if( visualization != _visualization )
		{
			_visualization = visualization;
			this->updateNodePoints( true );
			emit visualizationChanged( _visualization );
		}
	}
	void setSimilarityForHeight( bool enabled )
	{
		_similarityForHeight = enabled;
		this->updateNodePoints( true );
		emit similarityForHeightChanged( _similarityForHeight );
	}

	// Setter for the root node (dendrogram)
	void setRoot( const HCNode* root )
	{
		if( root != _root )
		{
			_root = root;
			_highlightedNodes.clear();
			_selectedNode = nullptr;

			this->updateNodePoints( false );
			emit rootChanged( _root );
			emit highlightedNodesChanged( _highlightedNodes );
			emit selectedNodeChanged( _selectedNode );
		}
	}

	// Setter for the similarity threshold for automatic node collapsing
	void setThreshold( float threshold )
	{
		for( auto& [node, value] : _nodeExpansion )	value = node->similarity() < threshold;
		this->updateNodePoints( false );
	}

	// Setters for different nodes
	void setHoveredNode( const HCNode* node )
	{
		if( node != _hoveredNode )
		{
			_hoveredNode = node;
			emit hoveredNodeChanged( _hoveredNode );
			this->update();
		}
	}
	void setSelectedNode( const HCNode* node )
	{
		if( node != _selectedNode )
		{
			_selectedNode = node;
			emit selectedNodeChanged( _selectedNode );
			this->updateNodePoints( false );
		}
	}
	void setHighlightedNodes( std::unordered_set<const HCNode*> nodes )
	{
		_highlightedNodes = std::move( nodes );
		emit highlightedNodesChanged( _highlightedNodes );
		this->update();
	}

private:
	// Size hints about the widget
	QSize sizeHint() const override
	{
		const auto textHeight = this->fontMetrics().height();
		const auto height = _TopPadding + 4 * ( _NodeSize + 2 * _Padding + textHeight );
		return QSize( 500, height );
	}

	// Recompute the node layout on resize
	void resizeEvent( QResizeEvent* event ) override
	{
		QWidget::resizeEvent( event );
		if( _root ) this->updateNodePoints( false );
	}

	// Render the dendrogram
	void paintEvent( QPaintEvent* event ) override
	{
		if( !_root ) return;

		auto painter = QPainter( this );
		painter.setRenderHint( QPainter::Antialiasing, true );

		// --- Draw Lines --- //
		painter.setPen( QPen( QColor( 218, 220, 224 ), 0.0 ) );
		for( const auto [node, newPoint] : _nodePoints ) if( !node->hasValue() )
		{
			if( _interpolation != 1.0f && _oldNodePoints.find( node ) == _oldNodePoints.end() ) continue;
			if( _interpolation != 1.0f && _oldNodePoints.find( node->left() ) == _oldNodePoints.end() ) continue;
			if( _nodePoints.find( node->left() ) == _nodePoints.end() ) continue;

			const auto oldPoint = _oldNodePoints[node];
			const auto point = oldPoint + _interpolation * ( newPoint - oldPoint );

			const auto oldLeft = _oldNodePoints[node->left()];
			const auto oldRight = _oldNodePoints[node->right()];

			const auto newLeft = _nodePoints[node->left()];
			const auto newRight = _nodePoints[node->right()];

			const auto left = oldLeft + _interpolation * ( newLeft - oldLeft );
			const auto right = oldRight + _interpolation * ( newRight - oldRight );

			painter.setRenderHint( QPainter::Antialiasing, false );
			painter.drawLine( left, QPoint( left.x(), point.y() ) );
			painter.drawLine( right, QPoint( right.x(), point.y() ) );
			painter.drawLine( left.x(), point.y(), right.x(), point.y() );
			painter.setRenderHint( QPainter::Antialiasing, true );
		}

		// --- Draw Nodes --- //
		for( const auto [node, newPoint] : _nodePoints )
		{
			if( _interpolation != 1.0f && _oldNodePoints.find( node ) == _oldNodePoints.end() ) continue;

			const auto oldPoint = _oldNodePoints[node];
			const auto point = oldPoint + _interpolation * ( newPoint - oldPoint );

			auto offset = QPoint( 2, 2 );
			if( _hoveredNode == node || _selectedNode == node ) offset += QPoint( 1, 1 );

			auto color = QColor( 218, 220, 224 ).darker( 120 );
			if( _selectedNode == node || _highlightedNodes.count( node ) ) color = QColor( 26, 115, 232 );
			else if( _nodeSelection[node] ) color = QColor( 200, 222, 249 );

			painter.setPen( Qt::transparent );
			painter.setBrush( color );
			painter.drawRect( QRect( point - offset, point + offset ) );

			if( !node->hasValue() && !_nodeExpansion[node] )
			{
				offset = QPoint( 1, 1 );
				painter.setBrush( QColor( 249, 200, 222 ) );
				painter.drawRect( QRect( point - offset, point + offset ) );
			}
		}

		// --- Draw Stats of Hovered Node --- //
		if( _hoveredNode )
		{
			auto text = QString();
			if( _hoveredNode->hasValue() ) text = "Volume " + QString::number( _hoveredNode->value() + 1 );
			else
			{
				text = "Volumes: " + QString::number( _hoveredNode->valueCount() ) +
					" (" + QString::number( 100.0 * _hoveredNode->valueCount() / _root->valueCount(), 'f', 1 ) +
					" %) | Similarity: " + QString::number( _hoveredNode->similarity(), 'f', 5 );
			}

			const auto textRect = this->rect().marginsRemoved( QMargins( 10, 10, 10, 10 ) );
			const auto alignment = _similarityForHeight ? Qt::AlignTop : Qt::AlignBottom;
			painter.setPen( QColor( 32, 33, 36 ) );
			painter.drawText( textRect, Qt::AlignLeft | alignment, text );
		}
	}

	// Handle mouse move events
	void mouseMoveEvent( QMouseEvent* event ) override
	{
		if( event->buttons() == Qt::NoButton )
			this->updateHoveredNode( event->pos() );
	}

	// Handles mouse button events
	void mousePressEvent( QMouseEvent* event ) override
	{
		if( !_hoveredNode ) return;

		if( event->button() == Qt::LeftButton )
		{
			// For the compressed view, holding shift when selecting a leaf will select the parent instead
			if( _visualization == Visualization::eCompressed && ( event->modifiers() & Qt::ShiftModifier ) && _hoveredNode->valueCount() == 1 )	_selectedNode = _hoveredNode->parent();
			else _selectedNode = _hoveredNode;

			emit selectedNodeChanged( _selectedNode );
		}

		// Right click to expand/collapse a node
		if( event->button() == Qt::RightButton ) _nodeExpansion[_hoveredNode] = !_nodeExpansion[_hoveredNode];
		
		// Update layout and hovered node
		this->updateNodePoints( false );
		this->updateHoveredNode( event->pos() );
		this->update();
	}

	// Reset the hovered node when the mouse leaves
	void leaveEvent( QEvent* event ) override
	{
		if( _hoveredNode )
		{
			_hoveredNode = nullptr;
			this->update();
		}
	}

	// Updates the node layout
	void updateNodePoints( bool animate )
	{
		_oldNodePoints = _nodePoints;
		_nodePoints.clear();

		int32_t x = 0;
		if( _visualization == Visualization::eComplete )
			this->updateNodePointsComplete( *_root, x, 0 );
		else if( _visualization == Visualization::eCompressed )
			this->updateNodePointsCompressed( *_root, x, 0 );

		if( animate ) _animation->start();
		else this->update();
	}

	// Recursively update the node layout using the "complete" visualization
	void updateNodePointsComplete( const HCNode& node, int32_t& x, int32_t layer )
	{
		if( _nodeExpansion.find( &node ) == _nodeExpansion.end() ) _nodeExpansion[&node] = true;
		_nodeSelection[&node] = ( &node == _selectedNode || _nodeSelection[node.parent()] );

		int32_t y = 0;
		if( _similarityForHeight )
		{
			const auto min = _root->similarity();
			const auto interp = ( node.similarity() - min ) / ( 1.0f - min );
			y = 5 + interp * ( height() - 10 );
		}
		else y = ( layer + 1 ) * 7;

		if( node.hasValue() || !_nodeExpansion[&node] )
		{
			x += 7;
			_nodePoints[&node] = QPoint( x, y );
		}
		else if( _nodeExpansion[&node] )
		{
			this->updateNodePointsComplete( *node.left(), x, layer + 1 );
			this->updateNodePointsComplete( *node.right(), x, layer + 1 );

			const auto left = _nodePoints[node.left()];
			const auto right = _nodePoints[node.right()];

			const auto x = ( left.x() + right.x() ) / 2;
			_nodePoints[&node] = QPoint( x, y );
		}
	}

	// Recursively update the node layout using the "compressed" visualization
	void updateNodePointsCompressed( const HCNode& node, int32_t& x, int32_t layer )
	{
		if( _nodeExpansion.find( &node ) == _nodeExpansion.end() ) _nodeExpansion[&node] = true;
		_nodeSelection[&node] = ( &node == _selectedNode || _nodeSelection[node.parent()] );

		int32_t y = 0;
		if( _similarityForHeight )
		{
			const auto min = _root->similarity();
			const auto interp = ( node.similarity() - min ) / ( 1.0f - min );
			y = 5 + interp * ( height() - 10 );
		}
		else y = ( layer + 1 ) * 7;

		if( node.hasValue() || !_nodeExpansion[&node] )
		{
			if( !node.hasValue() || &node == node.parent()->left() ) x += 7;
			_nodePoints[&node] = QPoint( x, y );
		}
		else if( _nodeExpansion[&node] )
		{
			this->updateNodePointsCompressed( *node.left(), x, layer + ( node.right()->valueCount() == 1 ? 2 : 1 ) );
			this->updateNodePointsCompressed( *node.right(), x, layer + 1 );

			const auto left = _nodePoints[node.left()];
			const auto right = _nodePoints[node.right()];

			const auto x = ( left.x() + right.x() ) / 2;

			_nodePoints[&node] = QPoint( x, y );
		}
	}

	// Update the currently hovered node
	void updateHoveredNode( QPoint cursor )
	{
		struct { const HCNode* node; double distance; } closest { nullptr, 10.0f };
		for( const auto [node, point] : _nodePoints )
		{
			const auto distance = QLineF( cursor, point ).length();
			if( distance < closest.distance ) closest = { node, distance };
		}

		if( closest.node != _hoveredNode )
		{
			_hoveredNode = closest.node;
			emit hoveredNodeChanged( _hoveredNode );
			this->update();
		}
	}

	Ensemble::SimilarityID _similarityID = Ensemble::SimilarityID( 0, Ensemble::Similarity::eField );
	Visualization _visualization = Visualization::eCompressed;
	const HCNode* _root = nullptr;

	const HCNode* _hoveredNode = nullptr;
	const HCNode* _selectedNode = nullptr;
	std::unordered_set<const HCNode*> _highlightedNodes;

	std::unordered_map<const HCNode*, QPoint> _oldNodePoints;
	std::unordered_map<const HCNode*, QPoint> _nodePoints;
	std::unordered_map<const HCNode*, bool> _nodeExpansion;
	std::unordered_map<const HCNode*, bool> _nodeSelection;

	bool _similarityForHeight = false;
	float _interpolation = 1.0f;
	QPropertyAnimation* _animation;

	static constexpr int32_t _TopPadding = 12;
	static constexpr int32_t _Padding = 8;
	static constexpr int32_t _NodeSize = 9;
};