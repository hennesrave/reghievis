#include "hierarchical_clustering.hpp"



HCNode::HCNode( int32_t value ) : _value( value ), _parent(), _left(), _right(), _similarity( 1.0f ), _valueCount( 1 ), _width( 1 ), _height( 1 ) {}
HCNode::HCNode( HCNode * left, HCNode * right, float similarity ) : _value(), _parent(), _left( left ), _right( right ), _similarity( similarity ), _valueCount( left->valueCount() + right->valueCount() )
{
	left->_parent = this;
	right->_parent = this;

	if( _left->valueCount() < _right->valueCount() ) std::swap( _left, _right );

	if( _right->valueCount() == 1 ) _width = _left->width();
	else _width = _left->width() + _right->width();

	_height = 1 + std::max( _left->height(), _right->height() );
}
HCNode::HCNode( int32_t count, const std::function<float( int32_t, int32_t )>&similarityFunction )
{
	// Initially, each value represents its own cluster
	auto nodes = std::vector<HCNode*>( count );
	for( int32_t i = 0; i < nodes.size(); ++i ) nodes[i] = new HCNode( i );

	// The similarity between two nodes using complete-linkage (the similarity is one minus the distance function, hence we use std::min)
	const auto computeSimilarity = [&similarityFunction] ( HCNode* first, HCNode* second )
	{
		auto similarity = std::numeric_limits<float>::max();
		for( const auto firstValue : first->values() )
			for( const auto secondValue : second->values() )
				similarity = std::min( similarity, similarityFunction( firstValue, secondValue ) );
		return similarity;
	};

	// Merge nodes until only one is left
	while( nodes.size() > 1 )
	{
		// Find two most similar nodes
		struct { int32_t first, second; float similarity; } closest { -1, -1, std::numeric_limits<float>::lowest() };
		for( int32_t i = 0; i < nodes.size(); ++i )
		{
			for( int32_t j = i + 1; j < nodes.size(); ++j )
			{
				const auto similarity = computeSimilarity( nodes[i], nodes[j] );
				if( similarity > closest.similarity ) closest = { i, j, similarity };
			}
		}

		// Merge two most similar nodes
		nodes.push_back( new HCNode( nodes[closest.first], nodes[closest.second], closest.similarity ) );
		nodes.erase( nodes.begin() + closest.second );
		nodes.erase( nodes.begin() + closest.first );
	}

	*this = std::move( *nodes.front() );
	delete nodes.front();
}
HCNode::HCNode( std::ifstream & stream ) : _value(), _parent(), _left(), _right(), _similarity(), _valueCount(), _width(), _height()
{
	// Read dendrogram recursively
	stream.read( reinterpret_cast<char*>( &_valueCount ), sizeof( int32_t ) );
	if( !_valueCount ) return;

	if( _valueCount == 1 )
	{
		_value = 0;
		_similarity = 1.0f;
		_width = 1;
		_height = 1;
		stream.read( reinterpret_cast<char*>( &_value.value() ), sizeof( int32_t ) );
	}
	else
	{
		stream.read( reinterpret_cast<char*>( &_similarity ), sizeof( _similarity ) );

		_left.reset( new HCNode( stream ) );
		_right.reset( new HCNode( stream ) );

		_left->_parent = this;
		_right->_parent = this;

		if( _right->valueCount() == 1 ) _width = _left->width();
		else _width = _left->width() + _right->width();

		_height = 1 + std::max( _left->height(), _right->height() );
	}
}

HCNode::HCNode( HCNode && other ) : _value( other._value ), _parent( nullptr ), _left( std::move( other._left ) ), _right( std::move( other._right ) ), _similarity( other._similarity ), _valueCount( other._valueCount ), _width( other._width ), _height( other._height )
{
	if( _left ) _left->_parent = this;
	if( _right ) _right->_parent = this;

	if( other._parent ) throw std::runtime_error( "Can't move HCNode with a parent." );
}

HCNode& HCNode::operator=( HCNode && other )
{
	if( other._parent ) throw std::runtime_error( "Can't move HCNode with a parent." );

	_value = other._value;
	_parent = nullptr;
	_left = std::move( other._left );
	_right = std::move( other._right );
	_similarity = other._similarity;
	_valueCount = other._valueCount;
	_width = other._width;
	_height = other._height;

	if( _left ) _left->_parent = this;
	if( _right ) _right->_parent = this;

	return *this;
}

void HCNode::save( std::ofstream & stream ) const
{
	// Save dendrogram recursively
	stream.write( reinterpret_cast<const char*>( &_valueCount ), sizeof( _valueCount ) );
	if( _value ) stream.write( reinterpret_cast<const char*>( &_value.value() ), sizeof( int32_t ) );
	else
	{
		stream.write( reinterpret_cast<const char*>( &_similarity ), sizeof( _similarity ) );
		_left->save( stream );
		_right->save( stream );
	}
}

bool HCNode::hasValue() const noexcept
{
	return _value.has_value();
}
int32_t HCNode::value() const
{
	return _value.value();
}

const HCNode* HCNode::parent() const noexcept
{
	return _parent;
}
HCNode* HCNode::parent() noexcept
{
	return _parent;
}

const HCNode* HCNode::left() const noexcept
{
	return _left.get();
}
HCNode* HCNode::left() noexcept
{
	return _left.get();
}

const HCNode* HCNode::right() const noexcept
{
	return _right.get();
}
HCNode* HCNode::right() noexcept
{
	return _right.get();
}

float HCNode::similarity() const noexcept
{
	return _similarity;
}

int32_t HCNode::valueCount() const noexcept
{
	return _valueCount;
}
std::vector<int32_t> HCNode::values() const
{
	auto values = std::vector<int32_t>();
	this->gatherValues( values );
	return values;
}

int32_t HCNode::width() const noexcept
{
	return _width;
}
int32_t HCNode::height() const noexcept
{
	return _height;
}

bool HCNode::operator==( const HCNode & other ) const
{
	if( _value && other._value ) return _value == other._value;
	else if( !_value && !other._value )
	{
		if( _similarity != other._similarity ) return false;
		if( _valueCount != other._valueCount ) return false;
		if( _width != other._width ) return false;
		if( _height != other._height ) return false;

		if( _left )
		{
			if( other._left )
			{
				if( *_left != *other._left ) return false;
			}
			else return false;
		}
		else if( other._left ) return false;

		if( _right )
		{
			if( other._right )
			{
				if( *_right != *other._right ) return false;
			}
			else return false;
		}
		else if( other._right ) return false;

		return true;
	}
	else return false;
}
bool HCNode::operator!=( const HCNode & other ) const
{
	return !( *this == other );
}

void HCNode::gatherValues( std::vector<int32_t>&values ) const
{
	// Recursively gather all leaf values
	if( _value ) values.push_back( *_value );
	else
	{
		_left->gatherValues( values );
		_right->gatherValues( values );
	}
}