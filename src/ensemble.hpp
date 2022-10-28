#pragma once
#include "qobject.h"

#include "common_widgets.hpp"
#include "hierarchical_clustering.hpp"
#include "volume.hpp"

#include <filesystem>
#include <map>
#include <set>

class Region;

// Class to manage an ensemble
class Ensemble
{
public:
	// Enum for the available derived volume types
	enum class Derived : int32_t
	{
		eNone, eMinimum, eMaximum, eMean, eStddev, eGradientMagnitude, ePCA1, ePCA2, eLabel,
		eHist1, eHist2, eHist3, eHist4, eHist5, eHistDeviation, eAndersonDarling
	};
	// Enum for the available similarity measures
	enum class Similarity : int32_t { eField, ePearson };

	// Struct to identify a type of volume based on its field, index (for member volumes), type (for derived volumes) and whether its a difference volume (for derived volumes)
	struct VolumeID
	{
		int32_t field;
		int32_t index;
		Ensemble::Derived type;
		bool difference = false;

		VolumeID( int32_t field = 0, int32_t index = 0 ) : field( field ), index( index ), type( Ensemble::Derived::eNone ), difference( false )
		{}
		VolumeID( int32_t field, Ensemble::Derived type, bool difference = false ) : field( field ), index( -1 ), type( type ), difference( difference )
		{}

		bool operator==( const VolumeID& other ) const noexcept
		{
			return field == other.field && index == other.index && type == other.type && difference == other.difference;
		}
		bool operator!=( const VolumeID& other ) const noexcept
		{
			return !( *this == other );
		}
		bool operator>( const VolumeID& other ) const noexcept
		{
			return field > other.field ? true : field == other.field ? ( index > other.index ? true : index == other.index ? type > other.type : false ) : false;
		}
		bool operator< ( const VolumeID& other ) const noexcept
		{
			return field < other.field ? true : field == other.field ? ( index < other.index ? true : index == other.index ? type < other.type : false ) : false;
		}

		struct hash
		{
			size_t operator()( const VolumeID& id ) const
			{
				uint64_t number = reinterpret_cast<const uint32_t&>( id.field );
				if( id.type == Ensemble::Derived::eNone ) number |= static_cast<uint64_t>( reinterpret_cast<const uint32_t&>( id.index ) ) << 32;
				else number |= static_cast<uint64_t>( reinterpret_cast<const uint32_t&>( id.type ) ) << 32;
				return util::hash_combine( std::hash<uint64_t>()( number ), id.difference );
			}
		};
	};

	// Struct to identify a type of dendrogram (HCNode) based on the field and type of similarity measure
	struct SimilarityID
	{
		int32_t field;
		Ensemble::Similarity similarity;

		SimilarityID( int32_t field = 0, Ensemble::Similarity similarity = Ensemble::Similarity::eField ) : field( field ), similarity( similarity )
		{}

		bool operator==( const SimilarityID& other ) const noexcept
		{
			return field == other.field && similarity == other.similarity;
		}
		bool operator!=( const SimilarityID& other ) const noexcept
		{
			return !( *this == other );
		}

		struct hash
		{
			size_t operator()( const SimilarityID& id ) const
			{
				uint64_t number = reinterpret_cast<const uint32_t&>( id.field );
				number |= static_cast<uint64_t>( reinterpret_cast<const uint32_t&>( id.similarity ) ) << 32;
				return std::hash<uint64_t>()( number );
			}
		};
	};

	// Class to manage a field
	class Field
	{
	public:
		Field() noexcept = default;
		Field( QString name ) noexcept;

		// Copy the specified volumes from the other field
		Field( const Field& other, const std::vector<int32_t>& volumes );

		// Copy the other field, applying a conversion (mapping) to the values of all members
		Field( const Field& other, QString name, const std::function<float( float )>& conversion );

		Field( const Field& ) = delete;
		Field( Field&& ) = default;

		Field& operator=( const Field& ) = delete;
		Field& operator=( Field&& ) = default;

		// Load different pre-defined fields
		void loadRFA();
		void loadTeardrop();
		void loadTangle();
		void loadSpheres();

		// Load or save field
		void load( std::ifstream& stream );
		void save( std::ofstream& stream ) const;

		// Compare to fields for equality
		bool compare( const Field& other ) const;

		void setName( QString name ) noexcept;
		const QString& name() const noexcept;

		// Getters for basic statistics
		int32_t memberCount() const noexcept;
		int32_t voxelCount() const noexcept;
		vec3i dimensions() const noexcept;

		// Getters for (derived) volumes
		const Volume<float>& volume( int32_t index ) const;
		const Volume<float>& volume( Derived derived ) const;
		const std::map<Derived, Volume<float>>& derivedVolumes() const noexcept;

		// Getters for similarity matrices and dendrograms
		const HCNode& root( Similarity similarity ) const;
		const std::map<Similarity, std::pair<Volume<float>, HCNode>>& similarites() const;

		// Compute the dendrogram using the specified similarity measure and voxels from the mask
		HCNode root( Similarity similarity, const Volume<float>& mask ) const;

		// Function to compute certain derived volumes
		void computeMinimumMaximum() const;
		void computeMeanStddev() const;
		void computeGradient() const;
		void computePrincipalComponents() const;
		void computeFieldSimilarity() const;
		void computePearsonSimilarity() const;
		void computeHistograms() const;
		void computeAndersonDarling() const;

	private:
		QString _name;
		std::vector<std::shared_ptr<Volume<float>>> _volumes;
		mutable std::map<Derived, Volume<float>> _derivedVolumes;
		mutable std::map<Similarity, std::pair<Volume<float>, HCNode>> _similarities;
		mutable Volume<vec3f> _volumeGradient;
	};

	// Returns a sub-ensemble using only the volumes with the given indices
	Ensemble createSubEnsemble( const std::vector<int32_t>& volumes ) const;

	// Load different pre-defined ensembles
	void loadRFA();
	void loadTeardrop();
	void loadTangle();
	void loadSpheres();

	// Load or save ensemble
	void load( std::filesystem::path filepath, bool computeDerivedVolumes );
	void save( std::filesystem::path filepath ) const;

	// Compare two ensembles for equality
	bool compare( const Ensemble& other ) const;

	// Getters for basic statistics about the ensemble
	int32_t fieldCount() const noexcept;
	int32_t memberCount() const noexcept;
	int32_t voxelCount() const noexcept;
	vec3i dimensions() const noexcept;

	// Getters for the label volume and fields
	const Volume<int32_t>& labels() const noexcept;
	const Field& field( int32_t index ) const noexcept;

	// Getters for (difference) volumes
	const Volume<float>& volume( const VolumeID& id ) const;
	const Volume<float>& differenceVolume( const VolumeID& id, const Ensemble& other ) const;

	// Getter for all available valid volume ids
	const std::set<VolumeID>& availableVolumes() const noexcept;

	// Getter for the root of a dendrogram
	const HCNode& root( const SimilarityID& id ) const;

	// Getter for all available ensemble types, e.g. types that are shared between multiple fields
	static inline const std::set<Ensemble::Derived>& ensembleTypes() noexcept
	{
		return _EnsembleTypes;
	}

private:
	std::filesystem::path _filepath;
	std::shared_ptr<Volume<int32_t>> _volumeLabels;
	std::vector<Field> _fields;

	mutable std::set<VolumeID> _availableVolumes;
	mutable std::map<Derived, Volume<float>> _derivedVolumes;
	mutable std::unordered_map<const Ensemble*, std::unordered_map<VolumeID, Volume<float>>> _differenceVolumes;

	static inline std::set<Ensemble::Derived> _EnsembleTypes = { Ensemble::Derived::eLabel };
};

// Function to convert Ensemble::Derived to a string
inline QString to_string( Ensemble::Derived type )
{
	switch( type )
	{
	case Ensemble::Derived::eNone: return "None";
	case Ensemble::Derived::eMinimum: return "Minimum";
	case Ensemble::Derived::eMaximum: return "Maximum";
	case Ensemble::Derived::eMean: return "Mean";
	case Ensemble::Derived::eStddev: return "Standard Deviation";
	case Ensemble::Derived::eGradientMagnitude: return "Gradient Magnitude";
	case Ensemble::Derived::ePCA1: return "Principal Component (1st)";
	case Ensemble::Derived::ePCA2: return "Principal Component (2nd)";
	case Ensemble::Derived::eLabel: return "Label";
	case Ensemble::Derived::eHist1: return "Z-Score Histogram (1st)";
	case Ensemble::Derived::eHist2: return "Z-Score Histogram (2nd)";
	case Ensemble::Derived::eHist3: return "Z-Score Histogram (3rd)";
	case Ensemble::Derived::eHist4: return "Z-Score Histogram (4th)";
	case Ensemble::Derived::eHist5: return "Z-Score Histogram (5th)";
	case Ensemble::Derived::eHistDeviation: return "Histogram Deviation";
	case Ensemble::Derived::eAndersonDarling: return "Anderson-Darling";
	default: return "Ensemble::Derived";
	}
}

// Hash function for Ensemble::VolumeID and Ensemble::SimilarityID to be able to use them in std::unordered_map etc.
namespace std
{
	template<> struct hash<Ensemble::VolumeID>
	{
		size_t operator()( const Ensemble::VolumeID& id ) const
		{
			return Ensemble::VolumeID::hash()( id );
		}
	};
	template<> struct hash<Ensemble::SimilarityID>
	{
		size_t operator()( const Ensemble::SimilarityID& id ) const
		{
			return Ensemble::SimilarityID::hash()( id );
		}
	};
}


// A common GUI widget for selecting a Ensemble::VolumeID
class VolumePicker : public QWidget
{
	Q_OBJECT
public:
	VolumePicker( const Ensemble& ensemble, bool addNoneOption, bool differenceEditable = false ) : QWidget()
	{
		auto layout = new QHBoxLayout( this );
		layout->setContentsMargins( 0, 0, 0, 0 );
		layout->setSpacing( 5 );

		layout->addWidget( _field = new ComboBox<int32_t> );
		layout->addWidget( _type = new ComboBox<Ensemble::Derived> );
		layout->addWidget( _difference = new CheckBox( false ) );

		for( int32_t field = 0; field < ensemble.fieldCount(); ++field )
		{
			_field->addItem( ensemble.field( field ).name(), field );

			if( addNoneOption ) _type->addItem( to_string( Ensemble::Derived::eNone ), Ensemble::Derived::eNone );

			for( const auto type : _Types )
				_type->addItem( to_string( type ), type );
		}

		_field->setVisible( ensemble.fieldCount() > 1 );
		_difference->setVisible( differenceEditable );

		const auto emitVolumeIDChanged = [=] { emit volumeIDChanged( this->volumeID() ); };
		QObject::connect( _field, &ComboBoxSignals::indexChanged, emitVolumeIDChanged );
		QObject::connect( _type, &ComboBoxSignals::indexChanged, emitVolumeIDChanged );
		QObject::connect( _difference, &CheckBox::stateChanged, emitVolumeIDChanged );
	}

	void setVolumeID( const Ensemble::VolumeID& id )
	{
		_field->blockSignals( true );
		_type->blockSignals( true );
		_difference->blockSignals( true );

		_field->setItem( id.field );
		_type->setItem( id.type );
		_difference->setChecked( id.difference );

		_field->blockSignals( false );
		_type->blockSignals( false );
		_difference->blockSignals( false );

		emit volumeIDChanged( this->volumeID() );
	}
	Ensemble::VolumeID volumeID() const
	{
		return Ensemble::VolumeID( _field->item(), _type->item(), _difference->checked() );
	}

	static inline const std::vector<Ensemble::Derived>& types() noexcept
	{
		return _Types;
	}

signals:
	void volumeIDChanged( Ensemble::VolumeID );

private:
	ComboBox<int32_t>* _field = nullptr;
	ComboBox<Ensemble::Derived>* _type = nullptr;
	CheckBox* _difference = nullptr;

	static inline std::vector<Ensemble::Derived> _Types = { Ensemble::Derived::eMinimum, Ensemble::Derived::eMaximum, Ensemble::Derived::eMean, Ensemble::Derived::eStddev, Ensemble::Derived::eGradientMagnitude, Ensemble::Derived::ePCA1, Ensemble::Derived::ePCA2, Ensemble::Derived::eLabel, Ensemble::Derived::eHistDeviation, Ensemble::Derived::eAndersonDarling };
};