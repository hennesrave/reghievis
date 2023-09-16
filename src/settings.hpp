#pragma once
#include <qcombobox.h>
#include <qfiledialog.h>
#include <qformlayout.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <qwidget.h>

#include <iostream>

#include "color_map.hpp"
#include "ensemble.hpp"
#include "dendrogram.hpp"
#include "parallel_coordinates.hpp"
#include "volume_renderer.hpp"

// Class that manages all the controls on the sidebar
class Settings : public QWidget
{
	Q_OBJECT
public:
	void initialize( const Ensemble& ensemble, ColorMapManager* colorMapManager, Dendrogram* dendrogram, VolumeRendererManager* volumeRendererManager, ParallelCoordinates* parallelCoordinates )
	{
		// Remember main widgets
		_ensemble = &ensemble;
		_colorMapManager = colorMapManager;
		_dendrogram = dendrogram;
		_volumeRendererManager = volumeRendererManager;
		_parallelCoordinates = parallelCoordinates;

		// Prepare layout
		auto layout = new QVBoxLayout( this );
		layout->setContentsMargins( 0, 0, 0, 0 );

		auto scrollArea = new QScrollArea();
		scrollArea->setWidgetResizable( true );
		scrollArea->setFrameShape( QFrame::NoFrame );
		scrollArea->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
		scrollArea->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
		layout->addWidget( scrollArea );

		auto scrollAreaWidget = new QWidget();
		scrollArea->setWidget( scrollAreaWidget );

		_layout = new QFormLayout( scrollAreaWidget );
		_layout->setContentsMargins( 10, 10, 10, 10 );
		_layout->setLabelAlignment( Qt::AlignVCenter | Qt::AlignRight );
		_layout->setVerticalSpacing( 5 );
		_layout->setHorizontalSpacing( 20 );

		// Save ensemble
		this->addSection( "General", QFont::Weight::Medium );
		auto saveEnsemble = new QPushButton( "Save Ensemble" );
		_layout->addRow( "", saveEnsemble);

		// Initialize different parts of the sidebar
		this->initializeDendrogram();
		this->initializeParallelCoordinates();
		this->initializeVolumeRenderer();

		// Initialize some connections
		const auto updateSampleColors = [=]
		{
			auto [unselected, selected] = _parallelCoordinates->sampleColors();
			unselected.setAlpha( std::min( 255.0, unselected.alpha() + _alphaBoost->value() ) );
			selected.setAlpha( std::min( 255.0, selected.alpha() + _alphaBoost->value() ) );
			_colorMapManager->setSampleColors( unselected, selected );
			_volumeRendererManager->settings().setHighlightedRegionColor( selected );
		};
		QObject::connect( saveEnsemble, &QPushButton::clicked, [=]
		{
			const auto filepath = QFileDialog::getSaveFileName( nullptr, "Save File", "../datasets" ).toStdString();
			if( !filepath.empty() ) _ensemble->save( filepath );
		} );
		QObject::connect( _parallelCoordinates, &ParallelCoordinates::sampleColorsChanged, updateSampleColors );
		QObject::connect( _alphaBoost, &NumberWidget::valueChanged, updateSampleColors );
		updateSampleColors();
	}

private:
	// Helper function for creating the heading of a new (sub-)section
	void addSection( const QString& title, QFont::Weight weight )
	{
		auto column = new QVBoxLayout();
		column->setContentsMargins( 0, 10, 0, 0 );
		column->setSpacing( 0 );

		auto label = new QLabel( title );
		label->setFont( QFont( "Roboto", 10, weight ) );
		column->addWidget( label );

		auto frame = new QFrame();
		frame->setFrameShape( QFrame::HLine );
		column->addWidget( frame );

		_layout->addRow( column );
	};

	void initializeDendrogram()
	{
		this->addSection( "Dendrogram", QFont::Weight::Medium );

		// Field selection
		auto field = new ComboBox<int32_t>();
		for( int32_t i = 0; i < _ensemble->fieldCount(); ++i )
			field->addItem( _ensemble->field( i ).name(), i );
		field->setItem( _dendrogram->similarityID().field );

		// Similarity measure selection
		auto similarity = new ComboBox<Ensemble::Similarity>();
		similarity->addItem( "Field Similarity", Ensemble::Similarity::eField );
		similarity->addItem( "Pearson Similarity", Ensemble::Similarity::ePearson );
		similarity->setItem( _dendrogram->similarityID().similarity );

		_layout->addRow( "Similarity Measure", util::createBoxLayout( QBoxLayout::LeftToRight, 5, { field, similarity } ) );
		field->setVisible( _ensemble->fieldCount() > 1 );

		// Region selection
		_regionsDendrogram = new ComboBox<Region*>();
		auto applyRegion = new QPushButton( "Apply" );
		_layout->addRow( "Region", util::createBoxLayout( QBoxLayout::LeftToRight, 5, { _regionsDendrogram, applyRegion }, { 1, 0 } ) );

		// Visualization type selection
		auto visualization = new ComboBox<Dendrogram::Visualization>();
		visualization->addItem( "Complete", Dendrogram::Visualization::eComplete );
		visualization->addItem( "Compressed", Dendrogram::Visualization::eCompressed );
		visualization->setItem( _dendrogram->visualization() );

		auto similarityForHeight = new CheckBox( false );
		_layout->addRow( "Visualization", util::createBoxLayout( QBoxLayout::LeftToRight, 5, { visualization, similarityForHeight }, { 1, 0 } ) );

		// Threshold selection and auto-layout
		auto threshold = new NumberWidget( 0.0, 1.0, 1.0, 0.00005, 5 );
		auto automaticLayout = new QPushButton( "Auto Layout" );
		_layout->addRow( "Similarity Threshold", util::createBoxLayout( QBoxLayout::LeftToRight, 5, { threshold, automaticLayout }, { 1, 0 } ) );

		// Initialize connections
		const auto updateSimilarity = [=] { _dendrogram->setSimilarity( Ensemble::SimilarityID( field->item(), similarity->item() ) ); };
		QObject::connect( field, &ComboBoxSignals::indexChanged, updateSimilarity );
		QObject::connect( similarity, &ComboBoxSignals::indexChanged, updateSimilarity );
		QObject::connect( applyRegion, &QPushButton::clicked, [=]
		{
			_regionRootNode = _ensemble->field( field->item() ).root( similarity->item(), *_regionsDendrogram->item()->createMask( *_ensemble ) );
			_dendrogram->setRoot( &_regionRootNode );
		} );
		QObject::connect( visualization, &ComboBoxSignals::indexChanged, [=]
		{
			_dendrogram->setVisualization( visualization->item() );
		} );
		QObject::connect( similarityForHeight, &CheckBox::stateChanged, _dendrogram, &Dendrogram::setSimilarityForHeight );
		QObject::connect( threshold, &NumberWidget::valueChanged, _dendrogram, &Dendrogram::setThreshold );
		QObject::connect( automaticLayout, &QPushButton::clicked, [=]
		{
			_volumeRendererManager->performAutomaticLayout( threshold->value() );
		} );

		QObject::connect( _dendrogram, &Dendrogram::rootChanged, [=] ( const HCNode* root )
		{
			if( root ) threshold->setMinimum( root->similarity() );
		} );
	}
	void initializeParallelCoordinates()
	{
		addSection( "Parallel Coordinates", QFont::Weight::Medium );

		// Region selection - Add initial region, notify widgets that require it
		_regions = new ItemList<Region*>( "Region" );
		_regions->addItem( "Region", new Region( "Region" ) );
		_regionsDendrogram->addItem( "Region", _regions->item() );
		_parallelCoordinates->setRegion( _regions->item() );
		_colorMapManager->setRegion( _regions->item() );
		_volumeRendererManager->setCurrentRegion( _regions->item() );
		_layout->addRow( "Region", _regions );

		// Binary combination of regions
		auto firstRegion = new ComboBox<Region*>();
		firstRegion->setMinimumContentsLength( 10 );
		firstRegion->addItem( "Region", _regions->item() );

		enum class CombineOp { eAnd, eOr, eXor, eAndNot, eOrNot };
		auto combineOp = new ComboBox<CombineOp>();
		combineOp->addItem( "and", CombineOp::eAnd );
		combineOp->addItem( "or", CombineOp::eOr );
		combineOp->addItem( "xor", CombineOp::eXor );
		combineOp->addItem( "and not", CombineOp::eAndNot );
		combineOp->addItem( "or not", CombineOp::eOrNot );

		auto secondRegion = new ComboBox<Region*>();
		secondRegion->setMinimumContentsLength( 10 );
		secondRegion->addItem( "Region", _regions->item() );

		auto buttonCombineRegions = new QPushButton();
		buttonCombineRegions->setObjectName( "icon" );
		buttonCombineRegions->setFixedSize( 20, 20 );
		buttonCombineRegions->setStyleSheet( "image: url(:/add.png)" );

		_layout->addRow( "", util::createBoxLayout( QBoxLayout::LeftToRight, 5, { firstRegion, combineOp, secondRegion, buttonCombineRegions }, { 1, 0, 1, 0 } ) );

		// Volume (axes) selection
		auto field = new ComboBox<int32_t>();
		auto volumeList = new ListView<Ensemble::VolumeID>();
		const auto fillVolumeList = [=]
		{
			volumeList->clear();
			if( _ensemble->fieldCount() > 1 )
			{
				if( field->item() == -1 ) for( const auto type : Ensemble::ensembleTypes() )
					volumeList->addItem( Ensemble::VolumeID( -1, type ), to_string( type ), true );
				else for( const auto type : VolumePicker::types() ) if( !Ensemble::ensembleTypes().count( type ) )
					volumeList->addItem( Ensemble::VolumeID( field->item(), type ), to_string( type ), true );
			}
			else for( const auto type : VolumePicker::types() )
				volumeList->addItem( Ensemble::VolumeID( Ensemble::ensembleTypes().count( type ) ? -1 : 0, type ), to_string( type ), true );

			if( field->item() >= 0 ) volumeList->addItem( Ensemble::VolumeID( field->item(), Ensemble::Derived::eHist1 ), "Z-Score Histogram", true );
		};

		if( _ensemble->fieldCount() > 1 )
		{
			for( int32_t i = 0; i < _ensemble->fieldCount(); ++i )
				field->addItem( _ensemble->field( i ).name(), i );
			field->addItem( "Ensemble", -1 );

			for( const auto type : VolumePicker::types() ) if( !Ensemble::ensembleTypes().count( type ) )
				volumeList->addItem( Ensemble::VolumeID( 0, type ), to_string( type ), true );
			volumeList->addItem( Ensemble::VolumeID( 0, Ensemble::Derived::eHist1 ), "Z-Score Histogram", true );

			_layout->addRow( "Volumes", field );
			_layout->addRow( "", volumeList );
		}
		else
		{
			field->addItem( _ensemble->field( 0 ).name(), 0 );
			for( const auto type : VolumePicker::types() )
				volumeList->addItem( Ensemble::VolumeID( Ensemble::ensembleTypes().count( type ) ? -1 : 0, type ), to_string( type ), true );
			volumeList->addItem( Ensemble::VolumeID( 0, Ensemble::Derived::eHist1 ), "Z-Score Histogram", true );
			_layout->addRow( "Volumes", volumeList );
		}

		// Axis spacing selection
		auto axisSpacing = new NumberWidget( 0, 100, 10 );
		_layout->addRow( "Axis Spacing", axisSpacing );

		// Sample count selection
		auto sampleCount = new NumberWidget( 0, 100, 100 );
		_layout->addRow( "Sample Count", sampleCount );

		// Clear selection button and realtime brushing checkbox
		auto clearSelection = new QPushButton( "Clear Selection" );
		auto realtimeEnabled = new CheckBox( false );
		_layout->addRow( "", util::createBoxLayout( QBoxLayout::LeftToRight, 5, { clearSelection, new QLabel( "Realtime" ), realtimeEnabled }, { 1, 0, 0 } ) );

		// Color selection for unselected/selected samples
		auto unselectedColor = new QPushButton( "Unselected" );
		auto selectedColor = new QPushButton( "Selected" );
		auto toggleColorMap = new QPushButton( "Color Map" );
		_layout->addRow( "Sample Color", util::createBoxLayout( QBoxLayout::LeftToRight, 10, { unselectedColor, selectedColor, toggleColorMap } ) );

		// Alpha boost for the color of samples in the scatter plot (2D color map editor)
		_alphaBoost = new NumberWidget( 0.0, 255.0, 15.0 );
		_layout->addRow( "2D Alpha Boost", _alphaBoost );

		// Initialize connections
		QObject::connect( _parallelCoordinates, &ParallelCoordinates::ensembleChanged, [=] ( const Ensemble* ensemble )
		{
			const auto oldMax = sampleCount->maximum();
			sampleCount->setMaximum( ensemble->voxelCount() );
			if( sampleCount->value() == oldMax )
				sampleCount->setValue( ensemble->voxelCount() );
		} );
		QObject::connect( _regions, &ItemListSignals::indexChanged, [=] ( int32_t index )
		{
			_parallelCoordinates->setRegion( _regions->item() );
			_colorMapManager->setRegion( _regions->item() );
			_volumeRendererManager->setCurrentRegion( _regions->item() );

			volumeList->blockSignals( true );
			volumeList->deselectAllItems();
			for( const auto volumeID : _regions->item()->enabledAxes() )
				volumeList->setItemState( volumeID, true );
			volumeList->blockSignals( false );
		} );
		QObject::connect( _regions, &ItemListSignals::itemNameChanged, [=] ( int32_t index, const QString& name )
		{
			auto item = _regions->item( index );
			item->setName( name );
			firstRegion->setText( index, name );
			secondRegion->setText( index, name );
			_regionsDendrogram->setText( index, name );
			_currentRegion->setText( _currentRegion->index( item ), name );
			_configRegions->setText( _configRegions->index( item ), name );
		} );
		QObject::connect( _regions, &ItemListSignals::itemAdded, [=] ( int32_t index )
		{
			auto& region = _regions->item( index );
			if( !region )
			{
				auto currentRegion = _regions->item();
				auto newRegion = currentRegion ? new Region( "Region", *currentRegion ) : new Region( "Region" );
				region = newRegion;
			}

			firstRegion->addItem( "Region", region );
			secondRegion->addItem( "Region", region );
			_regionsDendrogram->addItem( "Region", region );
			_configRegions->addItem( region, "Region", false );
		} );
		QObject::connect( _regions, &ItemListSignals::itemRemoved, [=] ( int32_t index )
		{
			_regions->item( index )->deleteLater();
			firstRegion->removeItem( index );
			secondRegion->removeItem( index );
			_regionsDendrogram->removeItem( index );
			_configRegions->removeItem( _regions->item( index ) );
		} );
		QObject::connect( buttonCombineRegions, &QPushButton::clicked, [=]
		{
			auto firstMask = firstRegion->item()->createMask( *_ensemble );
			auto secondMask = secondRegion->item()->createMask( *_ensemble );
			auto resultMask = std::make_shared<Volume<float>>( _ensemble->dimensions() );

			auto operation = std::function<bool( float, float )>();
			if( combineOp->item() == CombineOp::eAnd ) operation = [] ( float a, float b ) { return a && b; };
			else if( combineOp->item() == CombineOp::eOr ) operation = [] ( float a, float b ) { return a || b; };
			else if( combineOp->item() == CombineOp::eXor ) operation = [] ( float a, float b ) { return a != b; };
			else if( combineOp->item() == CombineOp::eAndNot ) operation = [] ( float a, float b ) { return a && !b; };
			else if( combineOp->item() == CombineOp::eOrNot ) operation = [] ( float a, float b ) { return a || !b; };

			util::compute_multi_threaded( 0, _ensemble->voxelCount(), [&] ( int32_t begin, int32_t end )
			{
				for( int32_t i = begin; i < end; ++i )
					resultMask->at( i ) = operation( firstMask->at( i ), secondMask->at( i ) );
			} );

			auto region = new Region( "Region", *firstRegion->item() );
			region->setConstantMask( resultMask );
			_regions->addItem( "Region", region );
		} );
		QObject::connect( field, &ComboBoxSignals::indexChanged, [=] ( int32_t index )
		{
			volumeList->blockSignals( true );
			fillVolumeList();
			volumeList->deselectAllItems();
			for( const auto volumeID : _regions->item()->enabledAxes() )
				volumeList->setItemState( volumeID, true );
			volumeList->blockSignals( false );
		} );
		QObject::connect( volumeList, &ListViewSignals::itemStateChanged, [=] ( int32_t index, bool selected )
		{
			_parallelCoordinates->setAxisEnabled( volumeList->item( index ), selected );
		} );
		QObject::connect( axisSpacing, &NumberWidget::valueChanged, [=] ( double value )
		{
			_parallelCoordinates->setAxisSpacing( value );
		} );
		QObject::connect( sampleCount, &NumberWidget::valueChanged, [=] ( double value )
		{
			_parallelCoordinates->setSampleCount( value );
			_colorMapManager->setSampleCount( value );
		} );
		QObject::connect( clearSelection, &QPushButton::clicked, _parallelCoordinates, &ParallelCoordinates::clearSelection );
		QObject::connect( realtimeEnabled, &CheckBox::stateChanged, _parallelCoordinates, &ParallelCoordinates::setRealtimeEnabled );
		QObject::connect( unselectedColor, &QPushButton::clicked, [=]
		{
			_parallelCoordinates->startEditingSampleColor( false );
		} );
		QObject::connect( selectedColor, &QPushButton::clicked, [=]
		{
			_parallelCoordinates->startEditingSampleColor( true );
		} );
		QObject::connect( toggleColorMap, &QPushButton::clicked, [=]
		{
			if( auto colorMap1D = _colorMapManager->currentColorMap1D(); colorMap1D && !colorMap1D->volumeID().difference )
				_parallelCoordinates->updateColorMap( colorMap1D );
			else if( auto colorMap2D = _colorMapManager->currentColorMap2D(); colorMap2D && !colorMap2D->volumeIDs().first.difference && !colorMap2D->volumeIDs().second.difference )
				_parallelCoordinates->updateColorMap( colorMap2D );
		} );
	}
	void initializeVolumeRenderer()
	{
		const auto& settings = _volumeRendererManager->settings();

		addSection( "Volume Rendering", QFont::Weight::Medium );

		// Row and column controls for the volume renderer grid
		auto addRow = new QPushButton( "Add" );
		auto removeRow = new QPushButton( "Remove" );
		_layout->addRow( "Rows", util::createBoxLayout( QBoxLayout::LeftToRight, 20, { addRow, removeRow } ) );

		auto addCol = new QPushButton( "Add" );
		auto removeCol = new QPushButton( "Remove" );
		_layout->addRow( "Columns", util::createBoxLayout( QBoxLayout::LeftToRight, 20, { addCol, removeCol } ) );

		// Interaction mode selection (viewing -> visualization, editing -> volume renderer arrangement and graph)
		auto interactionMode = new ComboBox<VolumeRendererManager::InteractionMode>();
		interactionMode->addItem( "Viewing", VolumeRendererManager::InteractionMode::eViewing );
		interactionMode->addItem( "Editing", VolumeRendererManager::InteractionMode::eEditing );
		_layout->addRow( "Mode", interactionMode );

		// Filtering for volume textures (GL_NEAREST vs GL_LINEAR)
		addSection( "Sampling", QFont::Weight::Light );
		auto filtering = new ComboBox<Filtering>();
		filtering->addItem( "Linear", Filtering::eLinear );
		filtering->addItem( "Nearest", Filtering::eNearest );
		_layout->addRow( "Filtering", filtering );

		// Composition mode selection
		auto compositing = new ComboBox<Compositing>();
		compositing->addItem( "Alpha Blending", Compositing::eAlphaBlending );
		compositing->addItem( "First Hit", Compositing::eFirstHit );
		compositing->addItem( "Maximum Intensity", Compositing::eMaximumIntensity );
		compositing->addItem( "First Local Maximum", Compositing::eFirstLocalMaximum );
		_layout->addRow( "Compositing", compositing );

		// Sample density selection (for ray casting)
		auto stepsPerVoxel = new NumberWidget( 1, 10, settings.stepsPerVoxel() );
		_layout->addRow( "Steps per Voxel", stepsPerVoxel );

		// Phong shading constants
		addSection( "Shading", QFont::Weight::Light );
		auto ambient = new NumberWidget( 0.0, 1.0, settings.shadingParams().x, 0.01, 2 );
		_layout->addRow( "Ambient", ambient );

		auto diffuse = new NumberWidget( 0.0, 1.0, settings.shadingParams().y, 0.01, 2 );
		_layout->addRow( "Diffuse", diffuse );

		auto specular = new NumberWidget( 0.0, 1.0, settings.shadingParams().z, 0.01, 2 );
		_layout->addRow( "Specular", specular );

		auto shininess = new NumberWidget( 1.0, 20.0, settings.shadingParams().w );
		_layout->addRow( "Shininess", shininess );

		// Configuration selection
		addSection( "Transfer Function", QFont::Weight::Light );
		auto regionCollections = new ItemList<QString>( "Configuration" );
		regionCollections->addItem( "Configuration", "" );
		_layout->addRow( "Configuration", regionCollections );

		// Region selection for current configuration
		_configRegions = new ListView<Region*>();
		_configRegions->addItem( _regions->item(), "Region", true );
		_configRegions->setItemSwappingEnabled( true );
		_configRegions->setMinSelectedItems( 1 );
		_layout->addRow( "Regions", _configRegions );

		// Current region selection for active regions
		_currentRegion = new ComboBox<const Region*>();
		_currentRegion->addItem( "Region", _regions->item() );
		_layout->addRow( "", _currentRegion );

		_volumeRendererManager->addRegionCollection();
		_volumeRendererManager->addRegion( 0, _regions->item() );

		// Color maps selection
		auto firstVolume = new VolumePicker( *_ensemble, false, true );
		_layout->addRow( "First Volume", firstVolume );

		auto secondVolume = new VolumePicker( *_ensemble, true, true );
		_layout->addRow( "Second Volume", secondVolume );

		auto colorMap1D = new ComboBox<ColorMap1D*>();
		_layout->addRow( "Color Map", colorMap1D );

		auto colorMap2D = new ComboBox<ColorMap2D*>();
		_layout->addRow( "Color Map 2D", colorMap2D );

		auto alphaVolume = new VolumePicker( *_ensemble, true, true );
		_layout->addRow( "Alpha Volume", alphaVolume );

		auto colorMap1DAlpha = new ComboBox<ColorMap1D*>();
		_layout->addRow( "Alpha Color Map", colorMap1DAlpha );

		// Initialize connections
		const auto updateShadingParams = [=]
		{
			_volumeRendererManager->settings().setShadingParams( vec4f( ambient->value(), diffuse->value(), specular->value(), shininess->value() ) );
		};
		const auto updateColorMaps = [=]
		{
			const auto first = firstVolume->volumeID();
			const auto second = secondVolume->volumeID();

			auto signalsBlocked = colorMap1D->signalsBlocked();
			colorMap1D->blockSignals( true );
			colorMap1D->clear();
			for( auto colorMap : _colorMapManager->colorMaps1D( first ) ) if( colorMap1D->index( colorMap ) == -1 )
			{
				colorMap1D->addItem( colorMap->name(), colorMap );
				if( !_connectedColorMaps.count( colorMap ) )
				{
					QObject::connect( colorMap, &ColorMap1D::nameChanged, [=]
					{
						colorMap1D->setText( colorMap1D->index( colorMap ), colorMap->name() );
						colorMap1DAlpha->setText( colorMap1DAlpha->index( colorMap ), colorMap->name() );
					} );
					_connectedColorMaps.insert( colorMap );
				}
			}
			colorMap1D->blockSignals( signalsBlocked );
			colorMap1D->indexChanged( colorMap1D->index() );

			signalsBlocked = colorMap2D->signalsBlocked();
			colorMap2D->blockSignals( true );
			colorMap2D->clear();
			if( second.type != Ensemble::Derived::eNone )
			{
				for( auto colorMap : _colorMapManager->colorMaps2D( first, second ) ) if( colorMap2D->index( colorMap ) == -1 )
				{
					colorMap2D->addItem( colorMap->name(), colorMap );
					if( !_connectedColorMaps.count( colorMap ) )
					{
						QObject::connect( colorMap, &ColorMap2D::nameChanged, [=]
						{
							colorMap2D->setText( colorMap2D->index( colorMap ), colorMap->name() );
						} );
						_connectedColorMaps.insert( colorMap );
					}
				}
			}
			colorMap2D->blockSignals( signalsBlocked );
			colorMap2D->indexChanged( colorMap2D->index() );
		};

		QObject::connect( addRow, &QPushButton::clicked, _volumeRendererManager, &VolumeRendererManager::addRow );
		QObject::connect( removeRow, &QPushButton::clicked, _volumeRendererManager, &VolumeRendererManager::removeRow );
		QObject::connect( addCol, &QPushButton::clicked, _volumeRendererManager, &VolumeRendererManager::addColumn );
		QObject::connect( removeCol, &QPushButton::clicked, _volumeRendererManager, &VolumeRendererManager::removeColumn );
		QObject::connect( interactionMode, &ComboBoxSignals::indexChanged, [=]
		{
			_volumeRendererManager->setInteractionMode( interactionMode->item() );
		} );
		QObject::connect( filtering, &ComboBoxSignals::indexChanged, [=]
		{
			_volumeRendererManager->settings().setFiltering( filtering->item() );
		} );
		QObject::connect( compositing, &ComboBoxSignals::indexChanged, [=]
		{
			_volumeRendererManager->settings().setCompositing( compositing->item() );
		} );
		QObject::connect( stepsPerVoxel, &NumberWidget::valueChanged, [=] ( double value )
		{
			_volumeRendererManager->settings().setStepsPerVoxel( value );
		} );
		QObject::connect( ambient, &NumberWidget::valueChanged, updateShadingParams );
		QObject::connect( diffuse, &NumberWidget::valueChanged, updateShadingParams );
		QObject::connect( specular, &NumberWidget::valueChanged, updateShadingParams );
		QObject::connect( shininess, &NumberWidget::valueChanged, updateShadingParams );

		QObject::connect( regionCollections, &ItemListSignals::indexChanged, [=] ( int32_t index )
		{
			_volumeRendererManager->setCurrentRegionCollection( index );

			_configRegions->blockSignals( true );
			_configRegions->deselectAllItems();

			_currentRegion->blockSignals( true );
			_currentRegion->clear();

			for( auto& info : _volumeRendererManager->regionCollection( index ).regions )
			{
				_configRegions->setItemState( const_cast<Region*>( info.region ), true );
				_currentRegion->addItem( info.region->name(), info.region );
			}

			_configRegions->blockSignals( false );

			_currentRegion->blockSignals( false );
			_currentRegion->indexChanged( _currentRegion->index() );
		} );
		QObject::connect( regionCollections, &ItemListSignals::itemNameChanged, [=] ( int32_t index, const QString& name )
		{
			_volumeRendererManager->setRegionCollectionName( index, name );
		} );
		QObject::connect( regionCollections, &ItemListSignals::itemAdded, [=]
		{
			_volumeRendererManager->addRegionCollection();
			_volumeRendererManager->addRegion( 0, _regions->item() );
			_volumeRendererManager->setColorMap1D( 0, colorMap1D->item() );
		} );
		QObject::connect( regionCollections, &ItemListSignals::itemRemoved, [=] ( int32_t index )
		{
			_volumeRendererManager->removeRegionCollection( index );
		} );
		QObject::connect( _configRegions, &ListViewSignals::itemStateChanged, [=] ( int32_t index, bool selected )
		{
			auto region = _configRegions->item( index );
			int32_t selectedIndex = 0;
			for( int32_t i = 0; i < index; ++i ) if( _configRegions->itemState( i ) ) ++selectedIndex;

			if( selected )
			{
				_volumeRendererManager->addRegion( selectedIndex, region );
				_volumeRendererManager->setColorMap1D( selectedIndex, colorMap1D->item( 0 ) );

				_currentRegion->addItem( selectedIndex, region->name(), region );
				_currentRegion->setIndex( selectedIndex );
			}
			else
			{
				_volumeRendererManager->removeRegion( _currentRegion->index( region ) );
				_currentRegion->removeItem( region );
			}
		} );
		QObject::connect( _configRegions, &ListViewSignals::itemsSwapped, [=] ( int32_t first, int32_t second )
		{
			if( _configRegions->itemState( first ) && _configRegions->itemState( second ) )
			{
				int32_t firstIndex = 0, secondIndex = 0;
				for( int32_t i = 0; i < first; ++i ) if( _configRegions->itemState( i ) ) ++firstIndex;
				for( int32_t i = 0; i < second; ++i ) if( _configRegions->itemState( i ) ) ++secondIndex;
				_currentRegion->swapItems( firstIndex, secondIndex );
				_volumeRendererManager->swapRegions( firstIndex, secondIndex );
			}
		} );
		QObject::connect( _configRegions, &ListViewSignals::itemRemoved, [=] ( int32_t index )
		{
			auto region = _configRegions->item( index );
			_volumeRendererManager->removeRegionForAll( region );
			_currentRegion->removeItem( region );
		} );
		QObject::connect( _currentRegion, &ComboBoxSignals::indexChanged, [=]
		{
			const auto index = _currentRegion->index();
			const auto& info = _volumeRendererManager->regionInfo( index );

			colorMap1D->blockSignals( true );
			colorMap2D->blockSignals( true );
			colorMap1DAlpha->blockSignals( true );

			firstVolume->setVolumeID( info.colorMap1D ? info.colorMap1D->volumeID() : Ensemble::VolumeID( 0, Ensemble::Derived::eNone ) );
			secondVolume->setVolumeID( info.colorMap2D ? info.colorMap2D->volumeIDs().second : Ensemble::VolumeID( 0, Ensemble::Derived::eNone ) );
			alphaVolume->setVolumeID( info.colorMap1DAlpha ? info.colorMap1DAlpha->volumeID() : Ensemble::VolumeID( 0, Ensemble::Derived::eNone ) );

			colorMap1D->setItem( const_cast<ColorMap1D*>( info.colorMap1D ) );
			colorMap2D->setItem( const_cast<ColorMap2D*>( info.colorMap2D ) );
			colorMap1DAlpha->setItem( const_cast<ColorMap1D*>( info.colorMap1DAlpha ) );

			colorMap1D->blockSignals( false );
			colorMap2D->blockSignals( false );
			colorMap1DAlpha->blockSignals( false );
		} );
		QObject::connect( firstVolume, &VolumePicker::volumeIDChanged, updateColorMaps );
		QObject::connect( secondVolume, &VolumePicker::volumeIDChanged, updateColorMaps );
		QObject::connect( alphaVolume, &VolumePicker::volumeIDChanged, [=]
		{
			const auto id = alphaVolume->volumeID();

			const auto signalsBlocked = colorMap1DAlpha->signalsBlocked();
			colorMap1DAlpha->blockSignals( true );
			colorMap1DAlpha->clear();
			if( id.type != Ensemble::Derived::eNone )
			{
				for( auto colorMap : _colorMapManager->colorMaps1D( id ) )
				{
					colorMap1DAlpha->addItem( colorMap->name(), colorMap );
					if( !_connectedColorMaps.count( colorMap ) )
					{
						QObject::connect( colorMap, &ColorMap1D::nameChanged, [=]
						{
							colorMap1D->setText( colorMap1D->index( colorMap ), colorMap->name() );
							colorMap1DAlpha->setText( colorMap1DAlpha->index( colorMap ), colorMap->name() );
						} );
						_connectedColorMaps.insert( colorMap );
					}
				}
			}
			colorMap1DAlpha->blockSignals( signalsBlocked );
			emit colorMap1DAlpha->indexChanged( colorMap1D->index() );
		} );

		QObject::connect( _colorMapManager, &ColorMapManager::colorMap1DAdded, [=] ( Ensemble::VolumeID volumeID, ColorMap1D* colorMap )
		{
			const auto first = firstVolume->volumeID();
			const auto second = alphaVolume->volumeID();

			if( volumeID == first && colorMap1D->index( colorMap ) == -1 )
			{
				colorMap1D->addItem( colorMap->name(), colorMap );
				QObject::connect( colorMap, &ColorMap1D::nameChanged, [=]
				{
					colorMap1D->setText( colorMap1D->index( colorMap ), colorMap->name() );
					colorMap1DAlpha->setText( colorMap1DAlpha->index( colorMap ), colorMap->name() );
				} );
				_connectedColorMaps.insert( colorMap );
			}
			if( volumeID == second && colorMap1DAlpha->index( colorMap ) == -1 )
			{
				colorMap1DAlpha->addItem( colorMap->name(), colorMap );
				QObject::connect( colorMap, &ColorMap1D::nameChanged, [=]
				{
					colorMap1D->setText( colorMap1D->index( colorMap ), colorMap->name() );
					colorMap1DAlpha->setText( colorMap1DAlpha->index( colorMap ), colorMap->name() );
				} );
				_connectedColorMaps.insert( colorMap );
			}
		} );
		QObject::connect( _colorMapManager, &ColorMapManager::colorMap1DRemoved, [=] ( Ensemble::VolumeID volumeID, ColorMap1D* colorMap )
		{
			colorMap1D->removeItem( colorMap );
			colorMap1DAlpha->removeItem( colorMap );
			_volumeRendererManager->replaceColorMap1D( colorMap, colorMap1D->item() );
		} );
		QObject::connect( _colorMapManager, &ColorMapManager::colorMap2DAdded, [=] ( Ensemble::VolumeID a, Ensemble::VolumeID b, ColorMap2D* colorMap )
		{
			const auto first = firstVolume->volumeID();
			const auto second = secondVolume->volumeID();

			if( ( a == first && b == second || a == second && b == first ) && colorMap2D->index( colorMap ) == -1 )
			{
				colorMap2D->addItem( colorMap->name(), colorMap );
				QObject::connect( colorMap, &ColorMap2D::nameChanged, [=]
				{
					colorMap2D->setText( colorMap2D->index( colorMap ), colorMap->name() );
				} );
				_connectedColorMaps.insert( colorMap );
			}
		} );
		QObject::connect( _colorMapManager, &ColorMapManager::colorMap2DRemoved, [=] ( Ensemble::VolumeID a, Ensemble::VolumeID b, ColorMap2D* colorMap )
		{
			colorMap2D->removeItem( colorMap );
			_volumeRendererManager->replaceColorMap2D( colorMap, colorMap2D->item() );
		} );

		QObject::connect( colorMap1D, &ComboBoxSignals::indexChanged, [=]
		{
			_volumeRendererManager->setColorMap1D( _currentRegion->index(), colorMap1D->item( nullptr ) );
		} );
		QObject::connect( colorMap2D, &ComboBoxSignals::indexChanged, [=]
		{
			_volumeRendererManager->setColorMap2D( _currentRegion->index(), colorMap2D->item( nullptr ) );
		} );
		QObject::connect( colorMap1DAlpha, &ComboBoxSignals::indexChanged, [=]
		{
			_volumeRendererManager->setColorMap1DAlpha( _currentRegion->index(), colorMap1DAlpha->item( nullptr ) );
		} );
	}

	const Ensemble* _ensemble = nullptr;
	ColorMapManager* _colorMapManager = nullptr;
	Dendrogram* _dendrogram = nullptr;
	VolumeRendererManager* _volumeRendererManager = nullptr;
	ParallelCoordinates* _parallelCoordinates = nullptr;

	QFormLayout* _layout = nullptr;

	HCNode _regionRootNode;
	std::unordered_set<QObject*> _connectedColorMaps;

	ItemList<Region*>* _regions = nullptr;
	ComboBox<Region*>* _regionsDendrogram = nullptr;
	ListView<Region*>* _configRegions = nullptr;
	ComboBox<const Region*>* _currentRegion = nullptr;

	NumberWidget* _alphaBoost = nullptr;
};