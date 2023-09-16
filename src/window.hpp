#pragma once
#include "color_map.hpp"
#include "dendrogram.hpp"
#include "parallel_coordinates.hpp"
#include "settings.hpp"
#include "volume_renderer.hpp"

#include "ensemble.hpp"

#include <qsplitter.h>

class Window : public QWidget
{
	Q_OBJECT
public:
	Window( std::filesystem::path filepath ) : QWidget(),
		_ensemble( new Ensemble() ),
		_colorMapManager( new ColorMapManager() ),
		_colorPicker( new ColorPicker() ),
		_dendrogram( new Dendrogram() ),
		_parallelCoordinates( new ParallelCoordinates ),
		_volumeRendererManager( new VolumeRendererManager( _ensemble, *_dendrogram, *_parallelCoordinates ) ),
		_settings( new Settings() )
	{
		// Load ensemble
		if( filepath == "teardrop" ) _ensemble->loadTeardrop();
		else if( filepath == "tangle" ) _ensemble->loadTangle();
		else if( filepath == "spheres" ) _ensemble->loadSpheres();
		else _ensemble->load( std::move( filepath ), false );

		// Layout main widgets
		auto row = util::createBoxLayout( QBoxLayout::LeftToRight, 0 );
		this->setLayout( row );
		row->addWidget( _settings, 0 );

		auto splitter = new QSplitter( Qt::Vertical );
		row->addWidget( splitter, 1 );

		auto splitter2 = new QSplitter( Qt::Horizontal );
		splitter2->addWidget( _dendrogram );
		splitter2->addWidget( _parallelCoordinates );
		splitter2->setStretchFactor( 1, 1 );
		splitter->addWidget( splitter2 );

		auto widget = new QWidget();
		row = util::createBoxLayout( QBoxLayout::LeftToRight, 0 );
		widget->setLayout( row );
		row->addWidget( _volumeRendererManager, 1 );

		auto column = util::createBoxLayout( QBoxLayout::TopToBottom, 0 );
		column->addWidget( new QWidget, 1 );
		column->addWidget( _colorMapManager );
		column->addWidget( _colorPicker );
		row->addLayout( column, 0 );
		splitter->addWidget( widget );
		splitter->setCollapsible( 0, false );
		splitter->setCollapsible( 1, false );
		splitter->setStretchFactor( 1, 1 );


		// Initialize widgets
		_settings->initialize( *_ensemble, _colorMapManager, _dendrogram, _volumeRendererManager, _parallelCoordinates );
		_colorMapManager->setEnsembles( *_ensemble, nullptr );
		_parallelCoordinates->setEnsemble( *_ensemble );
		_dendrogram->setRoot( &_ensemble->root( _dendrogram->similarityID() ) );
		_volumeRendererManager->settings().setClipRegion( std::make_pair( vec3i(), _ensemble->dimensions() ) );

		// Setup connection between widgets
		QObject::connect( _colorPicker, &ColorPicker::colorChanged, [=] ( QColor color )
		{
			if( _parallelCoordinates->editingSampleColor() ) _parallelCoordinates->setColor( color );
			else _colorMapManager->setColor( color );
		} );
		QObject::connect( _parallelCoordinates, &ParallelCoordinates::colorChanged, _colorPicker, &ColorPicker::setColor );
		QObject::connect( _colorMapManager, &ColorMapManager::colorChanged, [=] ( QColor color )
		{
			_parallelCoordinates->stopEditingSampleColor();
			_colorPicker->setColor( color );
		} );

		QObject::connect( _dendrogram, &Dendrogram::similarityIDChanged, [=] ( const Ensemble::SimilarityID& similarityID )
		{
			_dendrogram->setRoot( &_ensemble->root( similarityID ) );
		} );

		QObject::connect( _volumeRendererManager, &VolumeRendererManager::ensemblesChanged, [=] ( std::shared_ptr<Ensemble> ensemble, std::shared_ptr<Ensemble> other )
		{
			if( ensemble )
			{
				_colorMapManager->setEnsembles( *ensemble, other.get() );
				_parallelCoordinates->setEnsemble( *ensemble );
			}
		} );
		QObject::connect( _parallelCoordinates, &ParallelCoordinates::permutationBufferChanged, _colorMapManager, &ColorMapManager::setPermutationBuffer );
	}

private:
	std::shared_ptr<Ensemble> _ensemble;

	ColorMapManager* _colorMapManager = nullptr;
	ColorPicker* _colorPicker = nullptr;
	Dendrogram* _dendrogram = nullptr;
	ParallelCoordinates* _parallelCoordinates = nullptr;
	VolumeRendererManager* _volumeRendererManager = nullptr;
	Settings* _settings = nullptr;
};