#include "window.hpp"

#include <qapplication.h>
#include <qfile.h>
#include <qfont.h>
#include <qicon.h>
#include <qoffscreensurface.h>

#include <iostream>

int main( int argc, char** argv )
{
	try
	{
		// Set some global attributes
		QApplication::setAttribute( Qt::AA_EnableHighDpiScaling, true );
		QApplication::setAttribute( Qt::AA_ShareOpenGLContexts, true );
		QApplication::setFont( QFont( "Roboto", 10 ) );

		auto app = QApplication( argc, argv );

		// Create global OpenGL context so object can create OpenGL objects without inherting from QOpenGLWidget
		auto surface = QOffscreenSurface();
		surface.create();
		QOpenGLContext::globalShareContext()->makeCurrent( &surface );

		// Initialize global style sheet
		auto styleFile = QFile( ":/stylesheet.qss" );
		styleFile.open( QFile::ReadOnly );
		app.setStyleSheet( styleFile.readAll() );

		// Get dataset path
		QString filepath;
		if( argc > 1 ) filepath = QString( argv[1] );
		else if( filepath.isEmpty() ) filepath = QFileDialog::getOpenFileName( nullptr, "Open File", "../datasets" );
		if( filepath.isEmpty() ) return EXIT_SUCCESS;

		// Create and show man window
		auto window = Window( filepath.toStdString() );
		window.setWindowTitle( "Ensemble Visualization" );
		window.setWindowIcon( QIcon( ":/cube.png" ) );
		window.setMinimumSize( QSize( 1280, 720 ) );
		window.showMaximized();

		return app.exec();

	} catch( std::exception e )
	{
		std::cerr << "[Error]: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}