#include "ensemble.hpp"
#include "region.hpp"

#include <Eigen/Eigen>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>

#include <json.hpp>

Ensemble Ensemble::createSubEnsemble( const std::vector<int32_t>& volumes ) const
{
	// Create new ensemble, copy stuff that stays the same
	auto ensemble = Ensemble();
	ensemble._volumeLabels = _volumeLabels;
	ensemble._availableVolumes = _availableVolumes;

	// Copy the requested volumes from every field
	ensemble._fields = std::vector<Field>( _fields.size() );
	for( size_t i = 0; i < _fields.size(); ++i )
	{
		auto& field = ensemble._fields[i] = Field( _fields[i], volumes );

		// Pre-compute some basic derived volumes
		field.computeMinimumMaximum();
		field.computeMeanStddev();
		field.computeGradient();
	}

	return ensemble;
}

void Ensemble::loadRFA()
{
	auto timer = util::timer();

	// Load only field
	_fields.push_back( Field( "Value" ) );
	_fields.back().loadRFA();

	// Read labels
	auto istream = std::ifstream( "../../../Datasets/labels.data" );
	auto labels = std::vector<int32_t>( this->voxelCount() );
	for( int32_t i = 0; i < this->voxelCount(); ++i ) istream >> labels[i];
	istream.close();
	_volumeLabels.reset( new Volume<int32_t>( this->dimensions(), std::move( labels ), "Label" ) );

	// Fill available volumes
	for( const auto& [type, volume] : _fields.back().derivedVolumes() )
		_availableVolumes.insert( VolumeID( 0, type ) );
	_availableVolumes.insert( VolumeID( -1, Ensemble::Derived::eLabel ) );

	std::cout << "Finished loading 'RFA' with members = " << this->memberCount() << " and dimensions = " << this->dimensions() << " in " << timer.get() << " ms." << std::endl;
}
void Ensemble::loadTeardrop()
{
	auto timer = util::timer();

	// Load only field
	_fields.push_back( Field( "Value" ) );
	_fields.back().loadTeardrop();

	// No labels
	_volumeLabels.reset( new Volume<int32_t>( this->dimensions(), "Label" ));
	_volumeLabels->expandDomain( vec2i( 0, 1 ) );

	// Fill available volumes
	for( const auto& [type, volume] : _fields.back().derivedVolumes() )
		_availableVolumes.insert( VolumeID( 0, type ) );
	_availableVolumes.insert( VolumeID( -1, Ensemble::Derived::eLabel ) );

	std::cout << "Finished loading 'teardrop' with members = " << this->memberCount() << " and dimensions = " << this->dimensions() << " in " << timer.get() << " ms." << std::endl;
}
void Ensemble::loadTangle()
{
	auto timer = util::timer();

	// Load only field
	_fields.push_back( Field( "Value" ) );
	_fields.back().loadTangle();

	// No labels
	_volumeLabels.reset( new Volume<int32_t>( this->dimensions(), "Label" ));
	_volumeLabels->expandDomain( vec2i( 0, 1 ) );

	// Fill available volumes
	for( const auto& [type, volume] : _fields.back().derivedVolumes() )
		_availableVolumes.insert( VolumeID( 0, type ) );
	_availableVolumes.insert( VolumeID( -1, Ensemble::Derived::eLabel ) );

	std::cout << "Finished loading 'teardrop' with members = " << this->memberCount() << " and dimensions = " << this->dimensions() << " in " << timer.get() << " ms." << std::endl;
}
void Ensemble::loadSpheres()
{
	auto timer = util::timer();

	// Load only field
	_fields.push_back( Field( "Value" ) );
	_fields.back().loadSpheres();

	// No labels
	_volumeLabels.reset( new Volume<int32_t>( this->dimensions(), "Label" ));
	_volumeLabels->expandDomain( vec2i( 0, 1 ) );

	// Fill available volumes
	for( const auto& [type, volume] : _fields.back().derivedVolumes() )
		_availableVolumes.insert( VolumeID( 0, type ) );
	_availableVolumes.insert( VolumeID( -1, Ensemble::Derived::eLabel ) );

	std::cout << "Finished loading 'spheres' with members = " << this->memberCount() << " and dimensions = " << this->dimensions() << " in " << timer.get() << " ms." << std::endl;
}

void Ensemble::load( std::filesystem::path filepath, bool computeDerivedVolumes )
{
	auto timer = util::timer();
	
	// --- Open file stream --- //
	auto stream = std::ifstream( _filepath = std::move( filepath ), std::ios::in | std::ios::binary );
	if( !stream ) throw std::runtime_error( "Ensemble::load( std::filesystem::path ) -> Failed to open ensemble " + _filepath.string() );

	// --- Read label volume --- //
	_volumeLabels.reset( new Volume<int32_t>( stream ) );

	// --- Read derived volumes (on ensemble level) --- //
	const auto derivedVolumeCount = util::read_binary<size_t>( stream );
	for( size_t i = 0; i < derivedVolumeCount; ++i )
	{
		const auto key = util::read_binary<Derived>( stream );
		_derivedVolumes[key] = Volume<float>( stream );
	}

	// --- Read fields and compute derived volumes if requested --- //
	const auto fieldCount = util::read_binary<size_t>( stream );
	_fields = std::vector<Field>( fieldCount );
	for( size_t i = 0; i < fieldCount; ++i )
	{
		auto& field = _fields[i];
		field.load( stream );

		if( computeDerivedVolumes )
		{
			field.computeMinimumMaximum();
			field.computeMeanStddev();
			field.computeGradient();
			field.computePrincipalComponents();
			field.computeFieldSimilarity();
			field.computePearsonSimilarity();
			field.computeHistograms();
			field.computeAndersonDarling();
		}

		// --- Add available volume from field --- //
		for( const auto& [type, volume] : field.derivedVolumes() )
			_availableVolumes.insert( VolumeID( i, type ) );
	}

	// --- Add label volume to available volumes --- //
	_availableVolumes.insert( VolumeID( -1, Ensemble::Derived::eLabel ) );

	// --- Print status message --- //
	std::cout << "Finished loading " << _filepath.filename() << " with fields = " << this->fieldCount() << ", members = " << this->memberCount() << " and dimensions = " << this->dimensions() << " in " << timer.get() << " ms." << std::endl;
}
void Ensemble::save( std::filesystem::path filepath ) const
{
	auto timer = util::timer();

	// --- Open file stream --- //
	auto stream = std::ofstream( filepath, std::ios::out | std::ios::binary );

	// --- Save label volume --- //
	_volumeLabels->save( stream );

	// --- Save derived volumes (ensemble level) --- //
	util::write_binary( stream, _derivedVolumes.size() );
	for( const auto& [key, volume] : _derivedVolumes )
	{
		util::write_binary( stream, key );
		volume.save( stream );
	}

	// --- Save fields --- //
	util::write_binary( stream, _fields.size() );
	for( const auto& field : _fields ) field.save( stream );

	// --- Print status message --- //
	std::cout << "Finished saving " << filepath.filename() << " with fields = " << this->fieldCount() << ", members = " << this->memberCount() << " and dimensions = " << this->dimensions() << " in " << timer.get() << " ms." << std::endl;
}
bool Ensemble::compare( const Ensemble& other ) const
{
	// --- Compare label volumes --- //
	if( *_volumeLabels != *other._volumeLabels ) return false;
	std::cout << "Ensemble::compare( const Ensemble& ) -> Labels equal." << std::endl;

	// --- Compare derived volumes --- //
	if( _derivedVolumes != other._derivedVolumes ) return false;
	std::cout << "Ensemble::compare( const Ensemble& ) -> Derived volumes equal." << std::endl;

	// --- Compare fields --- //
	if( _fields.size() != other._fields.size() ) return false;
	for( size_t i = 0; i < _fields.size(); ++i ) if( !_fields[i].compare( other._fields[i] ) ) return false;
	std::cout << "Ensemble::compare( const Ensemble& ) -> Fields equal." << std::endl;

	std::cout << "Ensemble::compare( const Ensemble& ) -> Ensembles equal." << std::endl;
	return true;
}

int32_t Ensemble::fieldCount() const noexcept
{
	return _fields.size();
}
int32_t Ensemble::memberCount() const noexcept
{
	return _fields.empty() ? 0 : _fields.front().memberCount();
}
int32_t Ensemble::voxelCount() const noexcept
{
	return _fields.empty() ? 0 : _fields.front().voxelCount();
}
vec3i Ensemble::dimensions() const noexcept
{
	return _fields.empty() ? vec3i() : _fields.front().dimensions();
}

const Volume<int32_t>& Ensemble::labels() const noexcept
{
	return *_volumeLabels;
}
const Ensemble::Field& Ensemble::field( int32_t index ) const noexcept
{
	return _fields[index];
}

const Volume<float>& Ensemble::volume( const Ensemble::VolumeID& id ) const
{
	// If the requested volume has an ensemble type, search for it in derived volumes. Otherwise, return from specified field
	if( _EnsembleTypes.count( id.type ) )
	{
		if( _derivedVolumes.find( id.type ) == _derivedVolumes.end() )
		{
			switch( id.type )
			{
			case Derived::eLabel:
				_derivedVolumes[id.type] = _volumeLabels->cast<float>();
				_derivedVolumes[id.type].expandDomain( vec2f( 0.0f, 1.0f ) );
				break;
			}
		}
		return _derivedVolumes[id.type];
	}
	else
	{
		if( id.type == Ensemble::Derived::eNone ) return _fields[id.field].volume( id.index );
		else return _fields[id.field].volume( id.type );
	}
}
const Volume<float>& Ensemble::differenceVolume( const VolumeID& id, const Ensemble& other ) const
{
	// Return difference between two volumes of the same type from this and another ensemble. If it is not cached, compute it first.
	auto& volumes = _differenceVolumes[&other];
	if( volumes.count( id ) == 0 )
	{
		const auto& a = this->volume( id );
		const auto& b = other.volume( id );

		auto& result = volumes[id] = Volume<float>( a.dimensions(), a.name() + " (diff)" );
		util::compute_multi_threaded( 0, result.voxelCount(), [&] ( int32_t begin, int32_t end )
		{
			for( int32_t i = begin; i < end; ++i )
				result.at( i ) = a.at( i ) - b.at( i );
		} );
	}

	return _differenceVolumes[&other][id];
}

const HCNode& Ensemble::root( const SimilarityID& id ) const
{
	return _fields[id.field].root( id.similarity );
}
const std::set<Ensemble::VolumeID>& Ensemble::availableVolumes() const noexcept
{
	return _availableVolumes;
}





Ensemble::Field::Field( QString name ) noexcept : _name( std::move( name ) )
{}
Ensemble::Field::Field( const Field& other, const std::vector<int32_t>& volumes ) : _name( other._name )
{
	// Copy the specified volumes from the other field
	_volumes = std::vector<std::shared_ptr<Volume<float>>>( volumes.size() );
	for( int32_t i = 0; i < volumes.size(); ++i ) _volumes[i] = other._volumes[volumes[i]];
}
Ensemble::Field::Field( const Ensemble::Field& other, QString name, const std::function<float( float )>& conversion ) : _name( std::move( name ) )
{
	// Copy the other field, applying a mapping to the values of all members
	_volumes = std::vector<std::shared_ptr<Volume<float>>>( other._volumes.size() );
	for( int32_t i = 0; i < _volumes.size(); ++i ) _volumes[i] = std::make_shared<Volume<float>>( other._volumes[i]->map( conversion ) );
}

void Ensemble::Field::loadRFA()
{
	const auto directory = std::string( "../../../Datasets/ensemble" );

	auto data = std::unordered_map<std::string, std::pair<std::vector<float>, std::vector<float>>>();
	auto counter = 0;

	// --- Iterate all folders in the specified directory --- //
	for( const auto& it : std::filesystem::directory_iterator( directory ) )
	{
		const auto path = it.path();
		const auto extension = path.extension();

		// --- Look for files with either 'ParameterConfig' or 'TemperatureImage' in their name --- //
		auto id = path.filename().replace_extension().string();
		for( auto str : { std::string( "ParameterConfig" ), std::string( "TemperatureImage" ) } )
		{
			const auto pos = id.find( str );
			if( pos != std::string::npos ) id.replace( pos, str.length(), "" );
		}

		// --- For the '.bin' extension, read the temperature volume of the current member --- //
		if( extension == ".bin" )
		{
			auto stream = std::ifstream( path );
			auto values = std::vector<float>();
			std::copy( std::istream_iterator<float>( stream ), std::istream_iterator<float>(), std::back_inserter( values ) );
			data[id].second = std::move( values );
			std::cout << "Reading file '" << id << "' (" << ++counter << ")" << std::endl;
		}
		// --- For the '.json' extension, read the parameters of the current member --- //
		else if( extension == ".json" )
		{
			auto stream = std::ifstream( path );
			auto json = nlohmann::json();
			stream >> json;

			const auto& table = json["Material_Table"];
			const auto liver = std::stof( table[0]["ThermalConductivity"][0]["BaseValue"].get<std::string>() );
			const auto tumor = std::stof( table[1]["ThermalConductivity"][0]["BaseValue"].get<std::string>() );
			const auto vessel = std::stof( table[2]["ThermalConductivity"][0]["BaseValue"].get<std::string>() );

			auto values = std::vector<float> { liver, tumor, vessel };
			data[id].first = std::move( values );
		}
	}

	// --- Convert the read data into volumes --- //
	const auto dimensions = vec3i( 92, 92, 92 );
	for( auto& [key, value] : data )
	{
		auto volume = std::make_shared<Volume<float>>( dimensions, std::move( value.second ), "Value" );
		_volumes.push_back( volume );
	}
	_volumes.shrink_to_fit();

	// --- Compute all derived volumes --- //
	this->computeMinimumMaximum();
	this->computeMeanStddev();
	this->computeGradient();
	this->computePrincipalComponents();
	this->computeFieldSimilarity();
	this->computePearsonSimilarity();
	this->computeHistograms();
	this->computeAndersonDarling();
}
void Ensemble::Field::loadTeardrop()
{
	// Generate member volumes
	const auto dimensions = vec3i( 100, 100, 100 );
	_volumes = std::vector<std::shared_ptr<Volume<float>>>( 100 );

	// Allocate volumes
	for( size_t i = 0; i < _volumes.size(); ++i )
		_volumes[i].reset( new Volume<float>( dimensions ) );

	// Prepare noise
	auto noiseA = std::normal_distribution<float>( 0.0f, 0.05f );
	auto noiseB = std::normal_distribution<float>( 0.5f, 0.05f );
	auto noiseC = std::normal_distribution<float>( 1.0f, 0.05f );
	auto noiseD = std::normal_distribution<double>( 0.0, 0.001 );
	auto generator = std::mt19937( std::random_device( "12345" )( ) );

	auto offsets = std::vector<float>( _volumes.size() );
	for( size_t i = 0; i < _volumes.size(); ++i )
	{
		if( i < 0.5 * _volumes.size() ) offsets[i] = noiseA( generator );
		else if( i < 0.8 * _volumes.size() ) offsets[i] = noiseB( generator );
		else offsets[i] = noiseC( generator );
	}

	// Fill wil teardrop function
	util::compute_multi_threaded( 0, _volumes.size(), [&] ( int32_t begin, int32_t end )
	{
		for( int32_t i = begin; i < end; ++i )
		{
			auto& volume = _volumes[i];
			for( int32_t x = 0; x < volume->dimensions().x; ++x )
			{
				for( int32_t y = 0; y < volume->dimensions().y; ++y )
				{
					for( int32_t z = 0; z < volume->dimensions().z; ++z )
					{
						const auto v = vec3f( x, y, z ) / ( volume->dimensions() - 1 ) * 2.0f - 1.0f;
						const auto a = ( std::pow( v.x, 5 ) + std::pow( v.x, 4 ) ) / 2.0f - v.y * v.y - v.z * v.z;
						volume->at( vec3i( x, y, z ) ) = a + offsets[i] + noiseD( generator );
					}
				}
			}
		}
	} );

	// Compute all derived volumes
	this->computeMinimumMaximum();
	this->computeMeanStddev();
	this->computeGradient();
	this->computePrincipalComponents();
	this->computeFieldSimilarity();
	this->computePearsonSimilarity();
	this->computeHistograms();
	this->computeAndersonDarling();
}
void Ensemble::Field::loadTangle()
{
	auto timer = util::timer();

	// Generate member volumes
	const auto dimensions = vec3i( 100, 100, 100 );
	_volumes = std::vector<std::shared_ptr<Volume<float>>>( 100 );

	// Allocate volumes
	for( size_t i = 0; i < _volumes.size(); ++i )
		_volumes[i].reset( new Volume<float>( dimensions ) );

	// Prepare noise
	auto noiseA = std::normal_distribution<float>( 0.0f, 0.05f );
	auto noiseB = std::normal_distribution<float>( 5.0f, 0.05f );
	auto noiseC = std::normal_distribution<float>( -5.0f, 0.05f );
	auto noiseD = std::normal_distribution<double>( 0.0, 0.001 );
	auto generator = std::mt19937( std::random_device( "12345" )( ) );

	auto offsetsTop = std::vector<float>( _volumes.size() );
	auto offsetsBottom = std::vector<float>( _volumes.size() );
	for( size_t i = 0; i < _volumes.size(); ++i )
	{
		offsetsTop[i] = noiseA( generator );

		if( i < 0.5 * _volumes.size() ) offsetsBottom[i] = noiseB( generator );
		else offsetsBottom[i] = noiseC( generator );
	}

	// Fill wil tangle function
	const auto a = 0.0, b = -5.0, c = 11.8;
	util::compute_multi_threaded( 0, _volumes.size(), [&] ( int32_t begin, int32_t end )
	{
		for( int32_t i = begin; i < end; ++i )
		{
			auto& volume = _volumes[i];
			for( int32_t x = 0; x < volume->dimensions().x; ++x )
			{
				for( int32_t y = 0; y < volume->dimensions().y; ++y )
				{
					for( int32_t z = 0; z < volume->dimensions().z; ++z )
					{
						const auto v = ( vec3f( x, y, z ) / ( volume->dimensions() - 1 ) * 2.0f - 1.0f ) * 2.5f;

						// Fill wil tangle function
						const auto w = std::pow( v.x, 4 ) + std::pow( v.y, 4 ) + std::pow( v.z, 4 ) +
							a * std::pow( ( v.x * v.x + v.y * v.y + v.z * v.z ), 2 ) +
							b * ( v.x * v.x + v.y * v.y + v.z * v.z ) + c;
						volume->at( vec3i( x, y, z ) ) = w + ( v.y >= 0.0 ? offsetsTop[i] : offsetsBottom[i] ) + noiseD( generator );
					}
				}
			}
		}
	} );

	// Compute all derived volumes
	this->computeMinimumMaximum();
	this->computeMeanStddev();
	this->computeGradient();
	this->computePrincipalComponents();
	this->computeFieldSimilarity();
	this->computePearsonSimilarity();
	this->computeHistograms();
	this->computeAndersonDarling();
}
void Ensemble::Field::loadSpheres()
{
	auto timer = util::timer();

	// Generate member volumes
	const auto dimensions = vec3i( 100, 100, 100 );
	_volumes = std::vector<std::shared_ptr<Volume<float>>>( 150 );

	// Allocate volumes
	for( size_t i = 0; i < _volumes.size(); ++i )
		_volumes[i].reset( new Volume<float>( dimensions ) );

	// Prepare noise
	auto noise = std::normal_distribution<double>( 0.0, 0.001 );
	auto generator = std::mt19937( std::random_device( "12345" )( ) );

	// Fill wil spheres signed distance field
	util::compute_multi_threaded( 0, _volumes.size(), [&] ( int32_t begin, int32_t end )
	{
		for( int32_t i = begin; i < end; ++i )
		{
			auto& volume = _volumes[i];
			for( int32_t x = 0; x < volume->dimensions().x; ++x )
			{
				for( int32_t y = 0; y < volume->dimensions().y; ++y )
				{
					for( int32_t z = 0; z < volume->dimensions().z; ++z )
					{
						const auto v = vec3f( x, y, z ) / ( volume->dimensions() - 1 ) * 2.0f - 1.0f;

						// Fill wil tangle function
						auto center = vec3f();
						auto radius = 0.0f;
						if( i < 50 )
						{
							center = vec3f( 0.4f, 0.3f, 0.7f );
							radius = 0.2f;
						}
						else if( i < 100 )
						{
							center = vec3f( -0.6f, 0.0f, -0.2f );
							radius = 0.35f;
						}
						else
						{
							center = vec3f( -0.2f, -0.4f, -0.25f );
							radius = 0.25f;
						}

						const auto distance = ( v - center ).length() - radius;
						volume->at( vec3i( x, y, z ) ) = distance + noise( generator );
					}
				}
			}
		}
	} );

	// Compute all derived volumes
	this->computeMinimumMaximum();
	this->computeMeanStddev();
	this->computeGradient();
	this->computePrincipalComponents();
	this->computeFieldSimilarity();
	this->computePearsonSimilarity();
	this->computeHistograms();
	this->computeAndersonDarling();
}

void Ensemble::Field::load( std::ifstream& stream )
{
	// Read fild name
	std::string name;
	util::read_binary( stream, name );
	_name = QString::fromStdString( name );

	// Read member volumes
	_volumes = std::vector<std::shared_ptr<Volume<float>>>( util::read_binary<size_t>( stream ) );
	for( size_t i = 0; i < _volumes.size(); ++i ) _volumes[i].reset( new Volume<float>( stream ) );

	// Read derived volumes
	const auto derivedVolumeCount = util::read_binary<size_t>( stream );
	if( derivedVolumeCount ) for( size_t i = 0; i < derivedVolumeCount; ++i )
	{
		const auto key = util::read_binary<Derived>( stream );
		_derivedVolumes[key] = Volume<float>( stream );

		if( key == Derived::eGradientMagnitude ) _volumeGradient = Volume<vec3f>( stream );
	}
	else
	{
		this->computeMinimumMaximum();
		this->computeMeanStddev();
		this->computeGradient();
		this->computePrincipalComponents();
		this->computeHistograms();
		this->computeAndersonDarling();
	}

	// Read similarity matrices and the resulting dendrogram
	const auto similaritiesCount = util::read_binary<size_t>( stream );
	if( similaritiesCount ) for( size_t i = 0; i < similaritiesCount; ++i )
	{
		const auto key = util::read_binary<Similarity>( stream );
		auto volume = Volume<float>( stream );
		auto root = HCNode( stream );
		_similarities[key] = std::make_pair( std::move( volume ), std::move( root ) );
	}
	else
	{
		this->computeFieldSimilarity();
		this->computePearsonSimilarity();
	}
}
void Ensemble::Field::save( std::ofstream& stream ) const
{
	// Save the fiel name
	util::write_binary( stream, _name.toStdString() );

	// Save the member volumes
	util::write_binary( stream, _volumes.size() );
	for( const auto volume : _volumes ) volume->save( stream );

	// Save the derived volumes
	util::write_binary( stream, _derivedVolumes.size() );
	for( const auto& [key, volume] : _derivedVolumes )
	{
		util::write_binary( stream, key );
		volume.save( stream );

		if( key == Ensemble::Derived::eGradientMagnitude ) _volumeGradient.save( stream );
	}

	// Save the similarity matrices and resulting dendrograms
	util::write_binary( stream, _similarities.size() );
	for( const auto& [key, pair] : _similarities )
	{
		util::write_binary( stream, key );
		pair.first.save( stream );
		pair.second.save( stream );
	}
}
bool Ensemble::Field::compare( const Ensemble::Field& other ) const
{
	// Compare member volumes
	if( _volumes.size() != other._volumes.size() ) return false;
	for( size_t i = 0; i < _volumes.size(); ++i ) if( *_volumes[i] != *other._volumes[i] ) return false;
	std::cout << "Ensemble::Field::compare( const Ensemble::Field& ) -> Volumes equal." << std::endl;

	// Compare derived volumes
	if( _derivedVolumes != other._derivedVolumes ) return false;
	std::cout << "Ensemble::Field::compare( const Ensemble::Field& ) -> Derived volumes equal." << std::endl;

	// Compare similarity matrices and dendrograms
	if( _similarities != other._similarities ) return false;
	std::cout << "Ensemble::Field::compare( const Ensemble::Field& ) -> Similarities equal." << std::endl;

	// Compare gradient volume
	if( _volumeGradient != other._volumeGradient ) return false;
	std::cout << "Ensemble::Field::compare( const Ensemble::Field& ) -> Gradient volume equal." << std::endl;

	std::cout << "Ensemble::Field::compare( const Ensemble::Field& ) -> Fields equal." << std::endl;
	return true;
}

void Ensemble::Field::setName( QString name ) noexcept
{
	_name = std::move( name );
}
const QString& Ensemble::Field::name() const noexcept
{
	return _name;
}

int32_t Ensemble::Field::memberCount() const noexcept
{
	return _volumes.size();
}
int32_t Ensemble::Field::voxelCount() const noexcept
{
	return _volumes.empty() ? 0 : _volumes.front()->voxelCount();
}
vec3i Ensemble::Field::dimensions() const noexcept
{
	return _volumes.empty() ? vec3i() : _volumes.front()->dimensions();
}

const Volume<float>& Ensemble::Field::volume( int32_t index ) const
{
	return *_volumes[index];
}
const Volume<float>& Ensemble::Field::volume( Ensemble::Derived derived ) const
{
	// Return requested volume. If its not available, compute it first (lazy evaluation)
	if( _derivedVolumes.find( derived ) == _derivedVolumes.end() )
	{
		switch( derived )
		{
		case Derived::eMinimum:
		case Derived::eMaximum:
			this->computeMinimumMaximum();
			break;
		case Derived::eMean:
		case Derived::eStddev:
			this->computeMeanStddev();
			break;
		case Derived::eGradientMagnitude:
			this->computeGradient();
			break;
		case Derived::ePCA1:
		case Derived::ePCA2:
			this->computePrincipalComponents();
			break;
		case Derived::eLabel:
			throw std::invalid_argument( "Ensemble::Field::volume( Ensemble::Derived ) -> Ensemble::Derived::eLabel is an invalid argument." );
			break;
		case Derived::eHist1:
		case Derived::eHist2:
		case Derived::eHist3:
		case Derived::eHist4:
		case Derived::eHist5:
		case Derived::eHistDeviation:
			this->computeHistograms();
			break;
		case Derived::eAndersonDarling:
			this->computeAndersonDarling();
			break;
		}
	}

	return _derivedVolumes[derived];
}
const std::map<Ensemble::Derived, Volume<float>>& Ensemble::Field::derivedVolumes() const noexcept
{
	return _derivedVolumes;
}

const HCNode& Ensemble::Field::root( Ensemble::Similarity similarity ) const
{
	// Return requested dendrogram. If its not available, compute it first (lazy evaluation)
	if( _similarities.find( similarity ) == _similarities.end() )
	{
		switch( similarity )
		{
		case Similarity::eField:
			this->computeFieldSimilarity(); break;
		case Similarity::ePearson:
			this->computePearsonSimilarity(); break;
		}
	}

	return _similarities[similarity].second;
}
const std::map<Ensemble::Similarity, std::pair<Volume<float>, HCNode>>& Ensemble::Field::similarites() const
{
	return _similarities;
}

HCNode Ensemble::Field::root( Ensemble::Similarity similarity, const Volume<float>& mask ) const
{
	auto timer = util::timer();

	// Compute the similarity matrix and dendrogram using the specified similarity and only voxels where the mask is not zero
	// TODO: somehow merge with functions computeFielSimilarity and computePearsonSimilarity (DON'T REPEAT YOURSELF)

	auto similarityMatrix = Volume<float>( vec3i( this->memberCount(), this->memberCount(), 1 ), "Similarity" );
	std::fill( similarityMatrix.begin(), similarityMatrix.end(), 1.0f );

	auto voxels = std::vector<int32_t>();
	for( int32_t i = 0; i < mask.voxelCount(); ++i ) if( mask.at( i ) ) voxels.push_back( i );

	if( similarity == Ensemble::Similarity::eField )
	{
		util::compute_multi_threaded( 0, this->memberCount(), [&] ( int32_t begin, int32_t end )
		{
			for( int32_t i = begin; i < end; ++i )
			{
				if( begin == 0 ) std::cout << "Calculating field similarity (" << 100.0 * i / end << " %)          \r";
				for( int32_t j = i + 1; j < this->memberCount(); ++j )
				{
					const auto first = this->volume( i );
					const auto second = this->volume( j );

					const auto firstDomain = first.domain();
					const auto secondDomain = second.domain();

					const auto totalMin = std::min( firstDomain.x, secondDomain.x );
					const auto totalMax = std::max( firstDomain.y, secondDomain.y );
					const auto totalRange = totalMax - totalMin;

					double numerator = 0.0, denominator = 0.0;
					for( int32_t k : voxels )
					{
						const auto [min, max] = std::minmax( first.at( k ), second.at( k ) );

						numerator += 1.0 - ( max - totalMin ) / totalRange;
						denominator += 1.0 - ( min - totalMin ) / totalRange;
					}
					const auto similarity = denominator ? ( numerator / denominator ) : 1.0;
					similarityMatrix.at( vec3i( i, j, 0 ) ) = similarityMatrix.at( vec3i( j, i, 0 ) ) = static_cast<float>( similarity );
				}
			}
		} );
		std::cout << "Finished calculating field similarities!          " << std::endl;
	}
	else if( similarity == Ensemble::Similarity::ePearson )
	{

		auto means = std::vector<double>( this->memberCount() );
		auto stddevs = std::vector<double>( this->memberCount() );
		util::compute_multi_threaded( 0, this->memberCount(), [&] ( int32_t begin, int32_t end )
		{
			for( int32_t i = begin; i < end; ++i )
			{
				const auto volume = this->volume( i );

				auto& mean = means[i] = 0.0;
				for( int32_t j : voxels ) mean += volume.at( j );
				mean /= voxels.size();
			}
		} );
		util::compute_multi_threaded( 0, this->memberCount(), [&] ( int32_t begin, int32_t end )
		{
			for( int32_t i = begin; i < end; ++i )
			{
				const auto volume = this->volume( i );

				auto& stddev = stddevs[i] = 0.0;
				for( int32_t j : voxels )
				{
					const auto v = ( volume.at( j ) - means[i] );
					stddev += v * v;
				}
				stddev = std::sqrt( stddev );
			}
		} );
		util::compute_multi_threaded( 0, this->memberCount(), [&] ( int32_t begin, int32_t end )
		{
			for( int32_t i = begin; i < end; ++i )
			{
				if( begin == 0 ) std::cout << "Calculating pearson similarity (" << 100.0 * i / end << " %)          \r";
				for( int32_t j = i + 1; j < this->memberCount(); ++j )
				{
					const auto first = this->volume( i );
					const auto second = this->volume( j );

					auto correlation = 0.0;
					for( int32_t k : voxels )
						correlation += ( first.at( k ) - means[i] ) * ( second.at( k ) - means[j] );
					correlation = ( stddevs[i] == 0.0 && stddevs[j] == 0.0 ) ? 1.0 : ( correlation / ( stddevs[i] * stddevs[j] ) );

					const auto similarity = ( correlation + 1.0 ) / 2.0;
					similarityMatrix.at( vec3i( i, j, 0 ) ) = similarityMatrix.at( vec3i( j, i, 0 ) ) = static_cast<float>( similarity );
				}
			}
		} );
		std::cout << "Finished calculating pearson similarities!          " << std::endl;
	}

	const auto similarityFunction = [&] ( int32_t first, int32_t second )
	{
		return similarityMatrix.at( vec3i( first, second, 0 ) );
	};
	auto dendrogram = HCNode( this->memberCount(), similarityFunction );
	std::cout << "Finished clustering similarities in " << timer.get() << " ms." << std::endl;

	return dendrogram;
}

void Ensemble::Field::computeMinimumMaximum() const
{
	auto timer = util::timer();
	auto& minVolume = _derivedVolumes[Derived::eMinimum] = Volume<float>( this->dimensions(), "Minimum" );
	auto& maxVolume = _derivedVolumes[Derived::eMaximum] = Volume<float>( this->dimensions(), "Maximum" );

	util::compute_multi_threaded( 0, this->voxelCount(), [&] ( int32_t begin, int32_t end )
	{
		for( int32_t i = begin; i < end; ++i )
		{
			auto& min = minVolume.at( i ) = std::numeric_limits<float>::max();
			auto& max = maxVolume.at( i ) = std::numeric_limits<float>::lowest();

			for( int32_t j = 0; j < this->memberCount(); ++j )
			{
				const auto value = this->volume( j ).at( i );
				min = std::min( min, value );
				max = std::max( max, value );
			}
		}
	} );

	std::cout << "Finished computing minimum and maximum volumes in " << timer.get() << " ms." << std::endl;
}
void Ensemble::Field::computeMeanStddev() const
{
	auto timer = util::timer();

	auto& meanVolume = _derivedVolumes[Derived::eMean] = Volume<float>( this->dimensions(), "Mean" );
	auto& stddevVolume = _derivedVolumes[Derived::eStddev] = Volume<float>( this->dimensions(), "Stddev" );

	util::compute_multi_threaded( 0, this->voxelCount(), [&] ( int32_t begin, int32_t end )
	{
		for( int32_t i = begin; i < end; ++i )
		{
			double mean = 0.0;
			for( int32_t j = 0; j < this->memberCount(); ++j )
				mean += this->volume( j ).at( i );
			mean /= this->memberCount();

			meanVolume.at( i ) = static_cast<float>( mean );
		}
	} );
	util::compute_multi_threaded( 0, this->voxelCount(), [&] ( int32_t begin, int32_t end )
	{
		for( int32_t i = begin; i < end; ++i )
		{
			double stddev = 0.0;
			for( int32_t j = 0; j < this->memberCount(); ++j )
			{
				const auto value = this->volume( j ).at( i ) - meanVolume.at( i );
				stddev += value * value;
			}
			stddev = std::sqrt( stddev / this->memberCount() );

			stddevVolume.at( i ) = static_cast<float>( stddev );
		}
	} );

	std::cout << "Finished computing mean and stddev volumes in " << timer.get() << " ms." << std::endl;
}
void Ensemble::Field::computeGradient() const
{
	const auto timeBegin = std::chrono::high_resolution_clock::now();
	const auto& meanVolume = this->volume( Derived::eMean );

	_volumeGradient = Volume<vec3f>( this->dimensions(), "Gradient" );
	auto& magnitudeVolume = _derivedVolumes[Derived::eGradientMagnitude] = Volume<float>( this->dimensions(), "Gradient Magnitude" );

	util::compute_multi_threaded( 0, this->dimensions().x, [&] ( int32_t begin, int32_t end )
	{
		for( int32_t x = begin; x < end; ++x )
		{
			for( int32_t y = 0; y < this->dimensions().y; ++y )
			{
				for( int32_t z = 0; z < this->dimensions().z; ++z )
				{
					const auto voxel = vec3i( x, y, z );
					const auto center = meanVolume.at( voxel );

					auto gradient = vec3f();
					for( int32_t i = 0; i < 3; ++i )
					{
						vec3i forwardVoxel = voxel, backwardVoxel = voxel;
						forwardVoxel[i] += 1;
						backwardVoxel[i] -= 1;

						auto forward = 0.0f, backward = 0.0f, d = 2.0f;
						if( forwardVoxel[i] < this->dimensions()[i] ) forward = meanVolume.at( forwardVoxel );
						else forward = center, d = 1.0;
						if( backwardVoxel[i] >= 0 ) backward = meanVolume.at( backwardVoxel );
						else backward = center, d = 1.0;

						gradient[i] = ( forward - backward ) / d;
					}

					_volumeGradient.at( voxel ) = gradient;
				}
			}
		}
	} );
	util::compute_multi_threaded( 0, this->voxelCount(), [&] ( int32_t begin, int32_t end )
	{
		for( int32_t i = begin; i < end; ++i )
			magnitudeVolume.at( i ) = _volumeGradient.at( i ).length();
	} );

	const auto timeEnd = std::chrono::high_resolution_clock::now();
	const auto time = std::chrono::duration_cast<std::chrono::microseconds>( timeEnd - timeBegin ).count() / 1000.0;
	std::cout << "Finished computing gradient volumes in " << time << " ms." << std::endl;
}
void Ensemble::Field::computePrincipalComponents() const
{
	const auto timeBegin = std::chrono::high_resolution_clock::now();

	// Copy data to Eigen matrix
	using matrix = Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor>;
	auto data = matrix( this->voxelCount(), this->memberCount() );
	for( int32_t i = 0; i < this->memberCount(); ++i )
	{
		const auto& volume = this->volume( i );
		for( int32_t j = 0; j < this->voxelCount(); ++j )
			data( j, i ) = volume.at( j );
	}

	// Compute covariance matrix
	auto aligned = ( data.rowwise() - data.colwise().mean() ).eval();
	auto covariance = ( aligned.transpose() * aligned ).eval();

	// Perform singular value composition, receive projection matrix
	auto svd = Eigen::JacobiSVD<matrix>( covariance, Eigen::ComputeThinU );
	auto p = ( svd.matrixU().leftCols( 2 ) ).eval();

	// Project data points and normalize the values
	auto projected = ( aligned * p ).eval();
	projected = projected.rowwise() - projected.colwise().minCoeff();
	projected = projected.array().rowwise() / projected.colwise().maxCoeff().array();

	auto& pca1 = _derivedVolumes[Derived::ePCA1] = Volume<float>( this->dimensions(), "1st PC" );
	auto& pca2 = _derivedVolumes[Derived::ePCA2] = Volume<float>( this->dimensions(), "2nd PC" );

	// Copy projection from Eigen matrix to PCA volumes
	for( int32_t i = 0; i < this->voxelCount(); ++i )
	{
		pca1.at( i ) = projected( i, 0 );
		pca2.at( i ) = projected( i, 1 );
	}

	const auto timeEnd = std::chrono::high_resolution_clock::now();
	const auto time = std::chrono::duration_cast<std::chrono::microseconds>( timeEnd - timeBegin ).count() / 1000.0;
	std::cout << "Calculated principal component projection in " << time << " ms." << std::endl;
}
void Ensemble::Field::computeFieldSimilarity() const
{
	const auto timeBegin = std::chrono::high_resolution_clock::now();

	auto& fieldSimilarity = _similarities[Similarity::eField].first = Volume<float>( vec3i( this->memberCount(), this->memberCount(), 1 ), "Field Similarity" );
	std::fill( fieldSimilarity.begin(), fieldSimilarity.end(), 1.0f );

	// Compute field similarity matrix
	util::compute_multi_threaded( 0, this->memberCount(), [&] ( int32_t begin, int32_t end )
	{
		for( int32_t i = begin; i < end; ++i )
		{
			if( begin == 0 ) std::cout << "Calculating field similarity (" << 100.0 * i / end << " %)          \r";
			for( int32_t j = i + 1; j < this->memberCount(); ++j )
			{
				const auto first = this->volume( i );
				const auto second = this->volume( j );

				const auto firstDomain = first.domain();
				const auto secondDomain = second.domain();

				const auto totalMin = std::min( firstDomain.x, secondDomain.x );
				const auto totalMax = std::max( firstDomain.y, secondDomain.y );
				const auto totalRange = totalMax - totalMin;

				double numerator = 0.0, denominator = 0.0;
				for( int32_t k = 0; k < this->voxelCount(); ++k )
				{
					const auto [min, max] = std::minmax( first.at( k ), second.at( k ) );

					numerator += 1.0 - ( max - totalMin ) / totalRange;
					denominator += 1.0 - ( min - totalMin ) / totalRange;
				}
				const auto similarity = denominator ? ( numerator / denominator ) : 1.0;
				fieldSimilarity.at( vec3i( i, j, 0 ) ) = fieldSimilarity.at( vec3i( j, i, 0 ) ) = static_cast<float>( similarity );
			}
		}
	} );
	std::cout << "Finished calculating field similarities!          " << std::endl;

	// Create dendrogram using field similarity matrix
	const auto similarityFunction = [&] ( int32_t first, int32_t second )
	{
		return fieldSimilarity.at( vec3i( first, second, 0 ) );
	};
	_similarities[Similarity::eField].second = HCNode( this->memberCount(), similarityFunction );

	const auto timeEnd = std::chrono::high_resolution_clock::now();
	const auto time = std::chrono::duration_cast<std::chrono::microseconds>( timeEnd - timeBegin ).count() / 1000.0;
	std::cout << "Finished clustering field similarities in " << time << " ms." << std::endl;
}
void Ensemble::Field::computePearsonSimilarity() const
{
	const auto timeBegin = std::chrono::high_resolution_clock::now();

	auto& pearsonSimilarity = _similarities[Similarity::ePearson].first
		= Volume<float>( vec3i( this->memberCount(), this->memberCount(), 1 ), "Pearson Similarity" );
	std::fill( pearsonSimilarity.begin(), pearsonSimilarity.end(), 1.0f );

	// Compute Pearson similarity matrix
	auto means = std::vector<double>( this->memberCount() );
	auto stddevs = std::vector<double>( this->memberCount() );
	util::compute_multi_threaded( 0, this->memberCount(), [&] ( int32_t begin, int32_t end )
	{
		for( int32_t i = begin; i < end; ++i )
		{
			const auto volume = this->volume( i );

			auto& mean = means[i] = 0.0;
			for( int32_t j = 0; j < this->voxelCount(); ++j ) mean += volume.at( j );
			mean /= this->voxelCount();
		}
	} );
	util::compute_multi_threaded( 0, this->memberCount(), [&] ( int32_t begin, int32_t end )
	{
		for( int32_t i = begin; i < end; ++i )
		{
			const auto volume = this->volume( i );

			auto& stddev = stddevs[i] = 0.0;
			for( int32_t j = 0; j < this->voxelCount(); ++j )
			{
				const auto v = ( volume.at( j ) - means[i] );
				stddev += v * v;
			}
			stddev = std::sqrt( stddev );
		}
	} );
	util::compute_multi_threaded( 0, this->memberCount(), [&] ( int32_t begin, int32_t end )
	{
		for( int32_t i = begin; i < end; ++i )
		{
			if( begin == 0 ) std::cout << "Calculating pearson similarity (" << 100.0 * i / end << " %)          \r";
			for( int32_t j = i + 1; j < this->memberCount(); ++j )
			{
				const auto first = this->volume( i );
				const auto second = this->volume( j );

				auto correlation = 0.0;
				for( int32_t k = 0; k < this->voxelCount(); ++k )
					correlation += ( first.at( k ) - means[i] ) * ( second.at( k ) - means[j] );
				correlation = ( stddevs[i] == 0.0 && stddevs[j] == 0.0 ) ? 1.0 : ( correlation / ( stddevs[i] * stddevs[j] ) );

				const auto similarity = ( correlation + 1.0 ) / 2.0;
				pearsonSimilarity.at( vec3i( i, j, 0 ) ) = pearsonSimilarity.at( vec3i( j, i, 0 ) ) = static_cast<float>( similarity );
			}
		}
	} );
	std::cout << "Finished calculating pearson similarities!          " << std::endl;

	// Create dendrogram using Pearson similarity matrix
	const auto similarityFunction = [&] ( int32_t first, int32_t second )
	{
		return pearsonSimilarity.at( vec3i( first, second, 0 ) );
	};
	_similarities[Similarity::ePearson].second = HCNode( this->memberCount(), similarityFunction );

	const auto timeEnd = std::chrono::high_resolution_clock::now();
	const auto time = std::chrono::duration_cast<std::chrono::microseconds>( timeEnd - timeBegin ).count() / 1000.0;
	std::cout << "Finished clustering pearson similarities in " << time << " ms." << std::endl;
}
void Ensemble::Field::computeHistograms() const
{
	auto timer = util::timer();

	const auto& meanVolume = this->volume( Derived::eMean );
	const auto& stddevVolume = this->volume( Derived::eStddev );

	auto& hist1Volume = _derivedVolumes[Derived::eHist1] = Volume<float>( this->dimensions(), u8"z\u2011scores\u00A0in [-inf,-0.842]" );
	auto& hist2Volume = _derivedVolumes[Derived::eHist2] = Volume<float>( this->dimensions(), u8"z\u2011scores\u00A0in (-0.842,-0.253]" );
	auto& hist3Volume = _derivedVolumes[Derived::eHist3] = Volume<float>( this->dimensions(), u8"z\u2011scores\u00A0in (-0.253,0.253]" );
	auto& hist4Volume = _derivedVolumes[Derived::eHist4] = Volume<float>( this->dimensions(), u8"z\u2011scores\u00A0in (0.253,0.842]" );
	auto& hist5Volume = _derivedVolumes[Derived::eHist5] = Volume<float>( this->dimensions(), u8"z\u2011scores\u00A0in (0.842,inf]" );

	// Compute z-score histogram volumes
	util::compute_multi_threaded( 0, this->voxelCount(), [&] ( int32_t begin, int32_t end )
	{
		for( int32_t i = begin; i < end; ++i )
		{
			const auto mean = meanVolume.at( i );
			const auto stddev = stddevVolume.at( i );

			for( int32_t j = 0; j < this->memberCount(); ++j )
			{
				// Compute z-score, increment value of bin
				const auto z = stddev ? ( this->volume( j ).at( i ) - mean ) / stddev : 0.0;

				if( z <= -0.842 ) ++hist1Volume.at( i );
				else if( z <= -0.253 ) ++hist2Volume.at( i );
				else if( z <= 0.253 ) ++hist3Volume.at( i );
				else if( z <= 0.842 ) ++hist4Volume.at( i );
				else ++hist5Volume.at( i );
			}

			// Normalize values to [0, 1]
			hist1Volume.at( i ) /= this->memberCount();
			hist2Volume.at( i ) /= this->memberCount();
			hist3Volume.at( i ) /= this->memberCount();
			hist4Volume.at( i ) /= this->memberCount();
			hist5Volume.at( i ) /= this->memberCount();
		}
	} );

	// Make sure that the domain will always use [0, 1] (e.g. on parallel coordinates axes)
	hist1Volume.expandDomain( vec2f( 0.0f, 1.0f ) );
	hist2Volume.expandDomain( vec2f( 0.0f, 1.0f ) );
	hist3Volume.expandDomain( vec2f( 0.0f, 1.0f ) );
	hist4Volume.expandDomain( vec2f( 0.0f, 1.0f ) );
	hist5Volume.expandDomain( vec2f( 0.0f, 1.0f ) );

	auto& histDeviation = _derivedVolumes[Derived::eHistDeviation] = Volume<float>( this->dimensions(), to_string( Derived::eHistDeviation ) );
	util::compute_multi_threaded( 0, this->voxelCount(), [&] ( int32_t begin, int32_t end )
	{
		for( int32_t i = begin; i < end; ++i )
		{
			float max = 0.0f;
			for( const auto value : { hist1Volume.at( i ), hist2Volume.at( i ), hist3Volume.at( i ), hist4Volume.at( i ), hist5Volume.at( i ) } )
				max = std::max( max, std::abs( value - 0.2f ) );
			histDeviation.at( i ) = max;
		}
	} );
	histDeviation.expandDomain( vec2f( 0.0f, 0.8f ) );

	std::cout << "Finished computing histograms in " << timer.get() << " ms." << std::endl;
}
void Ensemble::Field::computeAndersonDarling() const
{
	auto timer = util::timer();

	const auto& meanVolume = this->volume( Derived::eMean );
	const auto& stddevVolume = this->volume( Derived::eStddev );

	auto& andersonDarlingVolume = _derivedVolumes[Derived::eAndersonDarling] = Volume<float>( this->dimensions(), "Anderson-Darling" );

	// Compute the cumulative distribution function of the normal distribution using std::erfc (https://stackoverflow.com/questions/2328258/cumulative-normal-distribution-function-in-c-c)
	const auto normalCDF = [] ( double v )
	{
		return 0.5 * std::erfc( -v * 0.707106781186547524401 );
	};

	// Calculate the Anderson-Darling test for every voxel (https://en.wikipedia.org/wiki/Anderson%E2%80%93Darling_test#Test_for_normality)
	util::compute_multi_threaded( 0, this->voxelCount(), [&] ( int32_t begin, int32_t end )
	{
		auto values = std::vector<float>( this->memberCount() );
		for( int32_t i = begin; i < end; ++i )
		{
			const auto mean = meanVolume.at( i );
			const auto stddev = stddevVolume.at( i );

			// Handle edge case
			if( stddev == 0.0 )
			{
				andersonDarlingVolume.at( i ) = 1.0f;
				continue;
			}

			// Standardize and sort values
			for( int32_t j = 0; j < this->memberCount(); ++j )
				values[j] = ( this->volume( j ).at( i ) - mean ) / stddev;
			std::sort( values.begin(), values.end() );

			auto A = 0.0;
			for( int32_t j = 0; j < this->memberCount(); ++j )
			{
				const auto o = j + 1;
				A += ( 2 * o - 1 ) * ( std::log( normalCDF( values[j] ) ) + std::log( 1.0 - normalCDF( values[this->memberCount() - o] ) ) );
			}
			A = -this->memberCount() - 1.0 / this->memberCount() * A;
			A = A * ( 1.0 + 0.75 / this->memberCount() - 2.25 / ( this->memberCount() * this->memberCount() ) );

			// Convert test statistic to p-value (https://www.spcforexcel.com/knowledge/basic-statistics/anderson-darling-test-for-normality)
			andersonDarlingVolume.at( i ) = A >= 0.6 ? std::exp( 1.2937 - 5.709 * A + 0.0186 * A * A )
				: A > 0.34 ? std::exp( 0.9177 - 4.279 * A - 1.38 * A * A )
				: A > 0.2 ? 1.0 - std::exp( -8.318 + 42.796 * A - 59.938 * A * A )
				: 1.0 - std::exp( -13.436 + 101.14 * A - 223.73 * A * A );

			// Handle edge case
			if( std::isnan( andersonDarlingVolume.at( i ) ) ) andersonDarlingVolume.at( i ) = 0.0f;
		}
	} );

	// Make sure that the domain will always use [0, 1] (e.g. on parallel coordinates axes)
	andersonDarlingVolume.expandDomain( vec2f( 0.0f, 1.0f ) );

	std::cout << "Finished computing Anderson-Darling in " << timer.get() << " ms." << std::endl;
}