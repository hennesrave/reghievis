#pragma once
#include <fstream>
#include <functional>
#include <memory>
#include <optional>

// Class for managing a node of dendrogram that resulted from a hierarchical clustering (HCNode <-> HierarchicalClusteringNode)
class HCNode
{
public:
public:
	HCNode() noexcept = default;

	// Create a leaf node containing the index to the volume
	HCNode( int32_t value );

	// Create an inner node with specified children and similarity
	HCNode( HCNode* left, HCNode* right, float similarity );

	// Create the root node of a dendrogram from the number of leaves (count) and using the specified similarity function
	HCNode( int32_t count, const std::function<float( int32_t, int32_t )>& similarityFunction );

	// Read a dendrogram from a file
	HCNode( std::ifstream& stream );

	// Copying dendrogram leads to problems, so prevent it
	HCNode( const HCNode& ) = delete;
	HCNode& operator=( const HCNode& ) = delete;

	// Move constructor and move assignment operator
	HCNode( HCNode&& other );
	HCNode& operator=( HCNode&& other );

	// Save a dendrogram to a file
	void save( std::ofstream& stream ) const;

	// Getter for the value of a leaf
	bool hasValue() const noexcept;
	int32_t value() const;

	// Getter for a nodes' parent
	const HCNode* parent() const noexcept;
	HCNode* parent() noexcept;

	// Getter for a nodes' children
	const HCNode* left() const noexcept;
	HCNode* left() noexcept;

	const HCNode* right() const noexcept;
	HCNode* right() noexcept;

	// Getter for a nodes similarity
	float similarity() const noexcept;

	// Getter for the (number of) values (leaves)
	int32_t valueCount() const noexcept;
	std::vector<int32_t> values() const;

	// Getter for the width (similar to value count, but is smaller due to the possibility to compress the dendrogram)
	int32_t width() const noexcept;

	// Getter for the height (number of levels) in this dendrogram
	int32_t height() const noexcept;

	// Compare two dendrograms
	bool operator==( const HCNode& other ) const;
	bool operator!=( const HCNode& other ) const;

private:
	// Gather the values from all leaves
	void gatherValues( std::vector<int32_t>& values ) const;

	std::optional<int32_t> _value;
	HCNode* _parent = nullptr;
	std::unique_ptr<HCNode> _left, _right;
	float _similarity = 0.0f;

	int32_t _valueCount = 0;
	int32_t _width = 0, _height = 0;
};