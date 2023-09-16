#pragma once
#include <qcombobox.h>
#include <qspinbox.h>
#include <qevent.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qpropertyanimation.h>
#include <qpushbutton.h>
#include <qwidget.h>

#include <any>
#include <iostream>

#include "math.hpp"
#include "utility.hpp"

// NOTE: to use Qt's signal & slots system with generic classes, the signals have to be outsourced to a parent class :/

// Class for a custom combo box (drop-down menu)
class ComboBoxSignals : public QWidget
{
	Q_OBJECT
signals:
	void indexChanged( int32_t );
};
template<typename T> class ComboBox : public ComboBoxSignals
{
public:
	ComboBox() : ComboBoxSignals(), _combobox( new QComboBox )
	{
		auto layout = new QHBoxLayout( this );
		layout->setContentsMargins( 0, 0, 0, 0 );
		layout->setSpacing( 0 );
		layout->addWidget( _combobox );

		QObject::connect( _combobox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this, &ComboBox::indexChanged );
	}

	void setIndex( int32_t index )
	{
		_combobox->setCurrentIndex( index );
	}
	void setItem( const T& item )
	{
		this->setIndex( this->index( item ) );
	}

	int32_t index() const
	{
		return _combobox->currentIndex();
	}
	int32_t index( const T& item )
	{
		const auto it = std::find( _items.begin(), _items.end(), item );
		return ( it == _items.end() ) ? -1 : it - _items.begin();
	}

	void setText( int32_t index, const QString& text )
	{
		if( index >= 0 ) _combobox->setItemText( index, text );
	}

	QString text( int32_t index ) const
	{
		return _combobox->itemText( index );
	}
	QString text() const
	{
		return this->text( this->index() );
	}

	void addItem( const QString& text, T item )
	{
		_items.push_back( std::move( item ) );
		_combobox->addItem( text );
	}
	void addItem( int32_t index, const QString& text, T item )
	{
		_items.insert( _items.begin() + index, std::move( item ) );
		_combobox->insertItem( index, text );
	}

	void removeItem( int32_t index )
	{
		if( index >= 0 )
		{
			_items.erase( _items.begin() + index );
			_combobox->removeItem( index );
		}
	}
	void removeItem( const T& item )
	{
		this->removeItem( this->index( item ) );
	}
	void clear()
	{
		_items.clear();
		_combobox->clear();
	}

	void swapItems( int32_t first, int32_t second )
	{
		if( first > second ) std::swap( first, second );

		const auto index = this->index();
		const auto firstText = this->text( first );
		const auto secondText = this->text( second );

		_combobox->blockSignals( true );
		_combobox->removeItem( second );
		_combobox->removeItem( first );
		_combobox->insertItem( first, secondText );
		_combobox->insertItem( second, firstText );
		if( index == first ) _combobox->setCurrentIndex( second );
		else if( index == second ) _combobox->setCurrentIndex( first );
		_combobox->blockSignals( false );

		std::swap( _items[first], _items[second] );
	}
	void swapItems( const T& first, const T& second )
	{
		this->swapItems( this->index( first ), this->index( second ) );
	}

	const T& item( int32_t index ) const
	{
		return _items[index];
	}
	T& item( int32_t index )
	{
		return _items[index];
	}
	const T& item() const
	{
		return this->item( this->index() );
	}
	T& item()
	{
		return this->item( this->index() );
	}
	const T& item( const T& default ) const
	{
		const auto index = this->index();
		return index == -1 ? default: this->item( index );
	}
	T& item( T& default )
	{
		const auto index = this->index();
		return index == -1 ? default: this->item( index );
	}

	int32_t itemCount() const noexcept
	{
		return _items.size();
	}

	void setMinimumContentsLength( int characters )
	{
		_combobox->setMinimumContentsLength( characters );
	}
	void setSizeAdjustPolicy( QComboBox::SizeAdjustPolicy policy )
	{
		_combobox->setSizeAdjustPolicy( policy );
	}

private:
	QComboBox* _combobox = nullptr;
	std::vector<T> _items;
};

// Class for a combo box where the user can add/remove items and change their names
class ItemListSignals : public QWidget
{
	Q_OBJECT
signals:
	void indexChanged( int32_t );
	void itemNameChanged( int32_t, const QString& );
	void itemAdded( int32_t );
	void itemRemoved( int32_t );
};
template<typename T> class ItemList : public ItemListSignals
{
public:
	ItemList( QString defaultText ) : ItemListSignals(), _defaultText( std::move( defaultText ) )
	{
		auto layout = new QHBoxLayout( this );
		layout->setContentsMargins( 0, 0, 0, 0 );
		layout->setSpacing( 2 );

		_comboBox = new QComboBox();
		_comboBox->setInsertPolicy( QComboBox::InsertAtCurrent );
		_comboBox->setEditable( true );
		layout->addWidget( _comboBox );

		_buttonAdd = new QPushButton();
		_buttonAdd->setObjectName( "icon" );
		_buttonAdd->setFixedSize( 16, 16 );
		_buttonAdd->setStyleSheet( "image: url(:/add.png)" );
		layout->addWidget( _buttonAdd );

		_buttonRemove = new QPushButton();
		_buttonRemove->setVisible( false );
		_buttonRemove->setObjectName( "icon" );
		_buttonRemove->setFixedSize( 16, 16 );
		_buttonRemove->setStyleSheet( "image: url(:/remove.png)" );
		layout->addWidget( _buttonRemove );

		QObject::connect( _comboBox, &QComboBox::editTextChanged, [=] ( const QString& text )
		{
			_comboBox->setItemText( _comboBox->currentIndex(), text );
			emit itemNameChanged( _comboBox->currentIndex(), text );
		} );
		QObject::connect( _comboBox, QOverload<int>::of( &QComboBox::currentIndexChanged ), this, &ItemListSignals::indexChanged );

		QObject::connect( _buttonAdd, &QPushButton::clicked, [=]
		{
			_comboBox->addItem( _defaultText );
			_items.push_back( T() );

			emit itemAdded( _comboBox->count() - 1 );

			_comboBox->setCurrentIndex( _comboBox->count() - 1 );

			if( _comboBox->count() > 1 ) _buttonRemove->setVisible( true );
		} );
		QObject::connect( _buttonRemove, &QPushButton::clicked, [=]
		{
			const auto index = _comboBox->currentIndex();
			emit itemRemoved( index );

			_items.erase( _items.begin() + index );
			_comboBox->removeItem( _comboBox->currentIndex() );

			if( _comboBox->count() == 1 ) _buttonRemove->setVisible( false );
		} );
	}

	void setIndex( int32_t index )
	{
		_comboBox->setCurrentIndex( index );
	}
	void setItem( const T& item )
	{
		this->setIndex( this->index( item ) );
	}

	int32_t index() const
	{
		return _comboBox->currentIndex();
	}
	int32_t index( const T& item )
	{
		const auto it = std::find( _items.begin(), _items.end(), item );
		return ( it == _items.end() ) ? -1 : it - _items.begin();
	}

	void setText( int32_t index, const QString& text )
	{
		if( index >= 0 ) _comboBox->setItemText( index, text );
	}

	QString text( int32_t index ) const
	{
		return _comboBox->itemText( index );
	}
	QString text() const
	{
		return this->text( this->index() );
	}

	void addItem( const QString& text, T item )
	{
		_items.push_back( std::move( item ) );
		_comboBox->addItem( text );
		emit itemAdded( _items.size() - 1 );
	}
	void removeItem( int32_t index )
	{
		if( index >= 0 )
		{
			emit itemRemoved( index );
			_items.erase( _items.begin() + index );
			_comboBox->removeItem( index );
		}
	}
	void removeItem( const T& item )
	{
		this->removeItem( this->index( item ) );
	}

	const T& item( int32_t index ) const
	{
		return _items[index];
	}
	T& item( int32_t index )
	{
		return _items[index];
	}
	const T& item() const
	{
		return this->item( this->index() );
	}
	T& item()
	{
		return this->item( this->index() );
	}
	const T& item( const T& default ) const
	{
		const auto index = this->index();
		return index == -1 ? default: this->item( index );
	}
	T& item( T& default )
	{
		const auto index = this->index();
		return index == -1 ? default: this->item( index );
	}

	int32_t itemCount() const noexcept
	{
		return _items.size();
	}

private:
	QString _defaultText;

	QComboBox* _comboBox = nullptr;
	QPushButton* _buttonAdd = nullptr;
	QPushButton* _buttonRemove = nullptr;

	std::vector<T> _items;
};

// Class for a list of items (displays multiple at a time) where the user can select/deselect items and rearrange them
class ListViewSignals : public QWidget
{
	Q_OBJECT
signals:
	void itemAdded( int32_t );
	void itemRemoved( int32_t );
	void itemStateChanged( int32_t, bool );
	void itemsSwapped( int32_t, int32_t );
};
template<typename T> class ListView : public ListViewSignals
{
	struct Item
	{
		T value;
		QString text;
		bool selected;
	};

public:
	ListView() : ListViewSignals()
	{
		this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Minimum );
		this->setMouseTracking( true );
	}

	int32_t index( const T& item )
	{
		const auto it = std::find_if( _items.begin(), _items.end(), [&] ( const Item& i ) { return i.value == item; } );
		return ( it == _items.end() ) ? -1 : it - _items.begin();
	}

	void setText( int32_t index, QString text )
	{
		if( index >= 0 )
		{
			_items[index].text = std::move( text );
			this->updateGeometry();
			this->update();
		}
	}
	const QString& text( int32_t index ) const
	{
		return _items[index].text;
	}

	void addItem( T item, QString text, bool selected = false )
	{
		// if selected item count is at maximum, don't select new item
		if( selected && this->selectedItemCount() >= _maxSelectedItems ) selected = false;

		_items.push_back( Item { item, text, selected } );
		emit itemAdded( _items.size() - 1 );
		this->updateGeometry();
		this->update();
	}
	void removeItem( int32_t index )
	{
		if( index >= 0 )
		{
			if( _minSelectedItems == _items.size() ) throw std::runtime_error( "ListView::removeItem( int32_t ) -> can't remove selected item when minimum selected item count is equal to total item count" );

			// If selected item count would fall below minimum, select first unselected item
			if( _items[index].selected && this->selectedItemCount() == _minSelectedItems )
				for( int32_t i = 0; i < _items.size(); ++i ) if( !_items[i].selected )
					this->setItemState( i, true );

			emit itemRemoved( index );
			_items.erase( _items.begin() + index );
			_begin = std::clamp( _begin, 0, std::max( 0, static_cast<int>( _items.size() - 5 ) ) );

			this->updateGeometry();
			this->update();
		}
	}
	void removeItem( const T& item )
	{
		this->removeItem( this->index( item ) );
	}
	void clear()
	{
		for( int32_t i = _items.size() - 1; i >= 0; --i )
			this->removeItem( i );
	}

	void setItemState( int32_t index, bool selected )
	{
		if( index >= 0 )
		{
			auto& item = _items[index];
			if( item.selected != selected )
			{
				const auto selectedItemCount = this->selectedItemCount();
				if( ( selected && selectedItemCount < _maxSelectedItems ) || ( !selected && selectedItemCount > _minSelectedItems ) )
				{
					item.selected = selected;
					emit itemStateChanged( index, selected );
					this->update();
				}
			}
		}
	}
	void setItemState( const T& item, bool selected )
	{
		this->setItemState( this->index( item ), selected );
	}
	void deselectAllItems()
	{
		// if( _minSelectedItems > 0 ) throw std::runtime_error( "ListView::deselectAllItems() -> can't deselect all items when minimum is not zero" );
		for( auto& item : _items ) item.selected = false;
		this->update();
	}

	bool itemState( int32_t index ) const
	{
		return _items[index].selected;
	}
	bool itemState( const T& item ) const
	{
		return this->itemState( this->index( item ) );
	}

	const T& item( int32_t index ) const
	{
		return _items[index].value;
	}
	T& item( int32_t index )
	{
		return _items[index].value;
	}
	const T& item( const T& default ) const
	{
		const auto index = this->index();
		return index == -1 ? default: this->item( index );
	}
	T& item( T& default )
	{
		const auto index = this->index();
		return index == -1 ? default: this->item( index );
	}

	int32_t itemCount() const noexcept
	{
		return _items.size();
	}
	int32_t selectedItemCount() const
	{
		int32_t counter = 0;
		for( const auto& item : _items ) if( item.selected ) ++counter;
		return counter;
	}

	int32_t minSelectedItems() const noexcept
	{
		return _minSelectedItems;
	}
	int32_t maxSelectedItems() const noexcept
	{
		return _maxSelectedItems;
	}

	void setMinSelectedItems( int32_t count )
	{
		if( count > _items.size() ) throw std::runtime_error( "ListView::setMinSelectedItems( int32_t ) -> count can't be higher than number of items" );

		_minSelectedItems = count;

		auto selectedItemCount = this->selectedItemCount();
		int32_t i = 0;
		while( selectedItemCount < _minSelectedItems )
		{
			if( !_items[i].selected )
			{
				this->setItemState( i, true );
				++selectedItemCount;
			}
			++i;
		}
	}
	void setMaxSelectedItems( int32_t count )
	{
		_maxSelectedItems = count;

		auto selectedItemCount = this->selectedItemCount();
		int32_t i = _items.size() - 1;
		while( selectedItemCount > _maxSelectedItems )
		{
			if( _items[i].selected )
			{
				this->setItemState( i, false );
				--selectedItemCount;
			}
			--i;
		}
	}

	bool isItemSwappingEnabled() const noexcept
	{
		return _itemSwappingEnabled;
	}
	void setItemSwappingEnabled( bool enabled )
	{
		_itemSwappingEnabled = enabled;
	}

	int32_t minItemDisplayCount() const noexcept
	{
		return _minItemDisplayCount;
	}
	int32_t maxItemDisplayCount() const noexcept
	{
		return _maxItemDisplayCount;
	}

	void setMinItemDisplayCount( int32_t count )
	{
		_minItemDisplayCount = count;
		this->updateGeometry();
		this->update();
	}
	void setMaxItemDisplayCount( int32_t count )
	{
		_maxItemDisplayCount = count;
		this->updateGeometry();
		this->update();
	}

private:
	QSize sizeHint() const override
	{
		auto width = 0;
		for( const auto& item : _items )
			width = std::max( width, 10 + this->fontMetrics().width( item.text ) );

		const auto textHeight = this->fontMetrics().height() + 4;
		return QSize( width, std::clamp( static_cast<int>( _items.size() ), 3, 5 ) * textHeight );
	}
	void paintEvent( QPaintEvent* event ) override
	{
		auto painter = QPainter( this );
		painter.setRenderHint( QPainter::Antialiasing, true );

		const auto textHeight = this->fontMetrics().height() + 4;

		for( int32_t i = _begin; i < _items.size(); ++i )
		{
			const auto& item = _items[i];

			const auto rect = QRect( 0, ( i - _begin ) * textHeight, this->width(), textHeight );
			auto background = QColor( 0, 0, 0, 0 );

			if( item.selected ) background = QColor( 200, 222, 249, 200 );
			else if( i == _hoveredItem ) background = QColor( 200, 222, 249, 100 );

			painter.fillRect( rect, background );
			painter.drawText( rect.marginsRemoved( QMargins( 5, 0, 5, 0 ) ), Qt::AlignVCenter | Qt::AlignLeft, item.text );
		}

		painter.setBrush( Qt::transparent );
		painter.setPen( QColor( 200, 222, 249 ) );
		painter.drawRect( 0, 0, this->width(), this->height() );
	}

	void mouseMoveEvent( QMouseEvent* event ) override
	{
		this->updateHoveredItem( event->pos() );
	}
	void leaveEvent( QEvent* event ) override
	{
		if( _hoveredItem != -1 )
		{
			_hoveredItem = -1;
			this->update();
		}
	}

	void mousePressEvent( QMouseEvent* event ) override
	{
		if( event->button() == Qt::LeftButton && _hoveredItem != -1 )
			_draggedItem = _hoveredItem;
	}
	void mouseReleaseEvent( QMouseEvent* event ) override
	{
		if( event->button() == Qt::LeftButton && _hoveredItem != -1 )
		{
			if( _hoveredItem == _draggedItem )
				this->setItemState( _hoveredItem, !_items[_hoveredItem].selected );
			else if( _itemSwappingEnabled )
			{
				std::swap( _items[_hoveredItem], _items[_draggedItem] );
				emit itemsSwapped( _draggedItem, _hoveredItem );
				this->update();
			}
		}
	}

	void wheelEvent( QWheelEvent* event ) override
	{
		event->accept();

		_begin += event->delta() > 0 ? -1 : 1;
		_begin = std::clamp( _begin, 0, std::max( 0, static_cast<int>( _items.size() - 5 ) ) );
		this->updateHoveredItem( event->pos() );
		this->update();
	}

	void updateHoveredItem( QPoint cursor )
	{
		const auto textHeight = this->fontMetrics().height() + 4;
		auto hoveredItem = -1;

		for( int32_t i = _begin; i < _items.size(); ++i )
		{
			const auto rect = QRect( 0, ( i - _begin ) * textHeight, this->width(), textHeight );
			if( rect.contains( cursor ) )
			{
				hoveredItem = i;
				break;
			}
		}

		if( hoveredItem != _hoveredItem )
		{
			_hoveredItem = hoveredItem;
			this->update();
		}
	}

	std::vector<Item> _items;

	int32_t _begin = 0;
	int32_t _hoveredItem = -1;

	int32_t _draggedItem = -1;
	bool _itemSwappingEnabled = false;

	int32_t _minSelectedItems = 0;
	int32_t _maxSelectedItems = std::numeric_limits<int32_t>::max();

	int32_t _minItemDisplayCount = 3;
	int32_t _maxItemDisplayCount = 5;
};



// Class for an animated check box
class CheckBox : public QWidget
{
	Q_OBJECT
	Q_PROPERTY( int position MEMBER _position );

public:
	CheckBox( bool checked );

	void setChecked( bool checked );
	bool checked() const noexcept;

signals:
	void stateChanged( bool );

private:
	QSize sizeHint() const override;

	void paintEvent( QPaintEvent* event ) override;
	void mouseReleaseEvent( QMouseEvent* event ) override;

	bool _checked;
	int _position;
	QPropertyAnimation* _animation;
};

// Class for custom slider
class Slider : public QWidget
{
	Q_OBJECT
public:
	Slider( bool integerSteps = true, double minimum = 0, double maximum = 100, double value = 0 );

	double value() const noexcept;
	double minimum() const noexcept;
	double maximum() const noexcept;
	vec2d range() const noexcept;

signals:
	void valueChanged( double );

public slots:
	void setValue( double value );
	void setMinimum( double minimum );
	void setMaximum( double maximum );
	void setRange( double minimum, double maximum );

private:
	QSize sizeHint() const override;
	void paintEvent( QPaintEvent* event ) override;

	void mouseMoveEvent( QMouseEvent* event ) override;
	void mousePressEvent( QMouseEvent* event ) override;

	void updateValue( QPoint cursor );

	static constexpr int32_t _Padding = 6;

	bool _integerSteps = true;
	double _minimum = 0, _maximum = 0, _value = 0;
};

// Class for a custom range slider (select lower and upper values)
class RangeSlider : public QWidget
{
	Q_OBJECT
public:
	RangeSlider( bool integerSteps = true, double minimum = 0, double maximum = 100, double lower = 0, double upper = 100 );

	bool hasIntegerSteps() const noexcept
	{
		return _integerSteps;
	}
	void setIntegerSteps( bool enabled )
	{
		_integerSteps = enabled;
	}

	double lower() const noexcept;
	double upper() const noexcept;
	vec2d values() const noexcept;

	double minimum() const noexcept;
	double maximum() const noexcept;
	vec2d range() const noexcept;

signals:
	void valuesChanged( double, double );

public slots:
	void setLower( double lower );
	void setUpper( double upper );
	void setValues( double lower, double upper );

	void setMinimum( double minimum );
	void setMaximum( double maximum );
	void setRange( double minimum, double maximum );

private:
	QSize sizeHint() const override;
	void paintEvent( QPaintEvent* event ) override;

	void mouseMoveEvent( QMouseEvent* event ) override;
	void mousePressEvent( QMouseEvent* event ) override;

	static constexpr int32_t _Padding = 6;

	bool _integerSteps = true;
	double _minimum = 0, _maximum = 0, _lower = 0, _upper = 0;
	enum { eNone, eLower, eUpper } _moving = eNone;
};



// Class for a slider combined with an input field
class NumberWidget : public QWidget
{
	Q_OBJECT
public:
	NumberWidget( double minimum, double maximum, double value, double stepSize, int precision );
	NumberWidget( double minimum = 0, double maximum = 100, double value = 0 );

	double value() const noexcept;
	double minimum() const noexcept;
	double maximum() const noexcept;
	vec2d range() const noexcept;

	void setStepSize( double stepSize );
	void setPrecision( int precision );

signals:
	void valueChanged( double );

public slots:
	void setValue( double value );
	void setMinimum( double minimum );
	void setMaximum( double maximum );
	void setRange( double minimum, double maximum );

private:
	QDoubleSpinBox* _spinbox = nullptr;
	Slider* _slider = nullptr;
};

// Class for a range slider with two input fields
class RangeWidget : public QWidget
{
	Q_OBJECT
public:
	RangeWidget( double minimum, double maximum, double lower, double upper, double stepSize, int precision ) :
		_lower( new QDoubleSpinBox ), _upper( new QDoubleSpinBox ), _slider( new RangeSlider( !precision, minimum, maximum, lower, upper ) )
	{
		auto layout = new QHBoxLayout( this );
		layout->setContentsMargins( 0, 0, 0, 0 );
		layout->setSpacing( 5 );

		auto row = new QHBoxLayout();
		row->setContentsMargins( 0, 0, 0, 0 );
		row->setSpacing( 2 );

		_lower->setButtonSymbols( QAbstractSpinBox::NoButtons );
		_lower->setRange( minimum, maximum );
		_lower->setValue( lower );
		_lower->setSingleStep( stepSize );
		_lower->setDecimals( precision );
		row->addWidget( _lower, 1 );

		row->addWidget( new QLabel( "-" ) );

		_upper->setButtonSymbols( QAbstractSpinBox::NoButtons );
		_upper->setRange( minimum, maximum );
		_upper->setValue( upper );
		_upper->setSingleStep( stepSize );
		_upper->setDecimals( precision );
		row->addWidget( _upper, 1 );
		layout->addLayout( row );

		layout->addWidget( _slider, 1 );

		QObject::connect( _lower, QOverload<double>::of( &QDoubleSpinBox::valueChanged ), _slider, &RangeSlider::setLower );
		QObject::connect( _upper, QOverload<double>::of( &QDoubleSpinBox::valueChanged ), _slider, &RangeSlider::setUpper );
		QObject::connect( _slider, &RangeSlider::valuesChanged, [=] ( double lower, double upper )
		{
			_lower->setValue( lower );
			_upper->setValue( upper );
		} );
		QObject::connect( _slider, &RangeSlider::valuesChanged, this, &RangeWidget::valuesChanged );
	}
	RangeWidget( double minimum = 0, double maximum = 100, double lower = 0, double upper = 100 )
		: RangeWidget( minimum, maximum, lower, upper, 1.0, 0.0 )
	{}

	double lower() const
	{
		return _slider->lower();
	}
	double upper() const
	{
		return _slider->upper();
	}
	vec2d values() const noexcept
	{
		return _slider->values();
	}

	double minimum() const
	{
		return _slider->minimum();
	}
	double maximum() const
	{
		return _slider->maximum();
	}
	vec2d range() const noexcept
	{
		return _slider->range();
	}

	void setStepSize( double stepSize )
	{
		_lower->setSingleStep( stepSize );
		_upper->setSingleStep( stepSize );
	}
	void setPrecision( int precision )
	{
		_lower->setDecimals( precision );
		_upper->setDecimals( precision );
		_slider->setIntegerSteps( !precision );
	}

signals:
	void valuesChanged( double, double );

public slots:
	void setLower( double lower )
	{
		_slider->setLower( lower );
	}
	void setUpper( double upper )
	{
		_slider->setUpper( upper );
	}
	void setValues( double lower, double upper )
	{
		_slider->setValues( lower, upper );
	}

	void setMinimum( double minimum )
	{
		_lower->setMinimum( minimum );
		_upper->setMinimum( minimum );
		_slider->setMinimum( minimum );
	}
	void setMaximum( double maximum )
	{
		_lower->setMaximum( maximum );
		_upper->setMaximum( maximum );
		_slider->setMaximum( maximum );
	}
	void setRange( double minimum, double maximum, bool updateStepSizeAndPrecision = false )
	{
		_lower->setRange( minimum, maximum );
		_upper->setRange( minimum, maximum );
		_slider->setRange( minimum, maximum );

		if( updateStepSizeAndPrecision )
		{
			auto onePercentStep = ( maximum - minimum ) / 100.0;
			auto singleStep = 1.0;
			auto precision = 1;
			while( onePercentStep && onePercentStep < 1.0 ) onePercentStep *= 10.0, singleStep /= 10.0, ++precision;

			this->setStepSize( singleStep );
			this->setPrecision( precision );
		}
	}

private:
	QDoubleSpinBox* _lower = nullptr;
	QDoubleSpinBox* _upper = nullptr;
	RangeSlider* _slider = nullptr;
};



// Classes for the HSV color picker
class HuePicker : public QWidget
{
	Q_OBJECT
public:
	HuePicker( int hue ) : QWidget(), _hue( hue )
	{
		this->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );

		if( _Image.width() == 0 )
		{
			_Image = QImage( 360, 1, QImage::Format_RGBA8888 );
			for( int32_t i = 0; i < _Image.width(); ++i )
				_Image.setPixelColor( i, 0, QColor::fromHsv( i, 255, 255 ) );
		}
	}

	int hue() const noexcept
	{
		return _hue;
	}

signals:
	void hueChanged( int hue );

public slots:
	void setHue( int hue )
	{
		hue = std::clamp( hue, 0, 359 );
		if( hue != _hue )
		{
			_hue = hue;
			this->update();
			emit hueChanged( hue );
		}
	}

private:
	QSize sizeHint() const override
	{
		return QSize( 360 + 20, 20 );
	}

	void paintEvent( QPaintEvent* event )override
	{
		auto painter = QPainter( this );
		painter.setRenderHint( QPainter::Antialiasing, true );

		painter.drawImage( QRect( 10, 6, 360, 8 ), _Image );

		painter.setPen( QPen( QColor( 255, 255, 255 ), 2.0 ) );
		painter.setBrush( QColor::fromHsv( _hue, 255, 255 ) );
		painter.drawEllipse( QRect( _hue, 0, 20, 20 ) );
	}
	void mouseMoveEvent( QMouseEvent* event ) override
	{
		if( event->buttons() == Qt::LeftButton )
		{
			_hue = std::clamp( event->x() - 10, 0, 359 );
			this->update();
			emit hueChanged( _hue );
		}
	}
	void mousePressEvent( QMouseEvent* event ) override
	{
		if( event->buttons() == Qt::LeftButton )
		{
			_hue = std::clamp( event->x() - 10, 0, 359 );
			this->update();
			emit hueChanged( _hue );
		}
	}

	static inline QImage _Image;
	int _hue = 0;
};

class SaturationValuePicker : public QWidget
{
	Q_OBJECT
public:
	SaturationValuePicker( int hue, int saturation, int value ) : QWidget(), _image( 256, 256, QImage::Format_RGBA8888 ),
		_hue( hue ), _saturation( saturation ), _value( value )
	{
		this->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );
		this->updateImage();
	}

	int hue() const noexcept
	{
		return _hue;
	}
	int saturation() const noexcept
	{
		return _saturation;
	}
	int value() const noexcept
	{
		return _value;
	}

signals:
	void hueChanged( int );
	void saturationValueChanged( int, int );

public slots:
	void setHue( int hue )
	{
		hue = std::clamp( hue, 0, 359 );
		if( hue != _hue )
		{
			_hue = hue;
			this->updateImage();
			emit hueChanged( _hue );
		}
	}
	void setSaturation( int saturation )
	{
		saturation = std::clamp( saturation, 0, 255 );
		if( saturation != _saturation )
		{
			_saturation = saturation;
			this->update();
			emit saturationValueChanged( _saturation, _value );
		}
	}
	void setValue( int value )
	{
		value = std::clamp( value, 0, 255 );
		if( value != _value )
		{
			_value = value;
			this->update();
			emit saturationValueChanged( _saturation, _value );
		}
	}

private:
	QSize sizeHint() const override
	{
		return QSize( 256 + 20, 256 + 20 );
	}

	void paintEvent( QPaintEvent* event )override
	{
		auto painter = QPainter( this );
		painter.setRenderHint( QPainter::Antialiasing, true );

		painter.drawImage( QRect( 10, 10, 256, 256 ), _image );

		painter.setPen( QPen( QColor( 255, 255, 255 ), 2.0 ) );
		painter.setBrush( QColor::fromHsv( _hue, _saturation, _value ) );
		painter.drawEllipse( QRect( _saturation, _value, 20, 20 ) );
	}
	void mouseMoveEvent( QMouseEvent* event ) override
	{
		if( event->buttons() == Qt::LeftButton )
		{
			_saturation = std::clamp( event->x() - 10, 0, 255 );
			_value = std::clamp( event->y() - 10, 0, 255 );
			this->update();
			emit saturationValueChanged( _saturation, _value );
		}
	}
	void mousePressEvent( QMouseEvent* event ) override
	{
		if( event->buttons() == Qt::LeftButton )
		{
			_saturation = std::clamp( event->x() - 10, 0, 255 );
			_value = std::clamp( event->y() - 10, 0, 255 );
			this->update();
			emit saturationValueChanged( _saturation, _value );
		}
	}

	void updateImage()
	{
		for( int32_t i = 0; i < _image.width(); ++i )
		{
			for( int32_t j = 0; j < _image.height(); ++j )
			{
				_image.setPixelColor( i, j, QColor::fromHsv( _hue, i, j ) );
			}
		}
		this->update();
	}

	QImage _image;
	int _hue = 0, _saturation = 0, _value = 0;
};

class ColorPicker : public QWidget
{
	Q_OBJECT
public:
	ColorPicker( QColor color = QColor::fromHsv( 0, 255, 255, 0 ) ) : QWidget(),
		_huePicker( new HuePicker( color.hue() ) ), _saturationValuePicker( new SaturationValuePicker( color.hue(), color.saturation(), color.value() ) ),
		_red( new QSpinBox ), _green( new QSpinBox ),
		_blue( new QSpinBox ), _alpha( new QSpinBox ),
		_handleRGB( true )
	{
		this->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );

		auto layoutA = new QBoxLayout( QBoxLayout::TopToBottom, this );
		layoutA->setContentsMargins( 10, 10, 10, 10 );
		layoutA->setSpacing( 0 );
		layoutA->addWidget( _huePicker );

		auto layoutB = new QBoxLayout( QBoxLayout::LeftToRight );
		layoutB->addWidget( _saturationValuePicker );

		auto layoutC = new QBoxLayout( QBoxLayout::TopToBottom );

		auto row = new QBoxLayout( QBoxLayout::LeftToRight );
		row->addWidget( new QLabel( "Red" ) );
		_red->setButtonSymbols( QAbstractSpinBox::NoButtons );
		_red->setRange( 0, 255 );
		_red->setValue( color.red() );
		row->addWidget( _red );
		layoutC->addLayout( row );

		row = new QBoxLayout( QBoxLayout::LeftToRight );
		row->addWidget( new QLabel( "Green" ) );
		_green->setButtonSymbols( QAbstractSpinBox::NoButtons );
		_green->setRange( 0, 255 );
		_green->setValue( color.green() );
		row->addWidget( _green );
		layoutC->addLayout( row );

		row = new QBoxLayout( QBoxLayout::LeftToRight );
		row->addWidget( new QLabel( "Blue" ) );
		_blue->setButtonSymbols( QAbstractSpinBox::NoButtons );
		_blue->setRange( 0, 255 );
		_blue->setValue( color.blue() );
		row->addWidget( _blue );
		layoutC->addLayout( row );

		row = new QBoxLayout( QBoxLayout::LeftToRight );
		row->addWidget( new QLabel( "Alpha" ) );
		_alpha->setButtonSymbols( QAbstractSpinBox::NoButtons );
		_alpha->setRange( 0, 255 );
		_alpha->setValue( color.alpha() );
		row->addWidget( _alpha );
		layoutC->addLayout( row );

		layoutB->addLayout( layoutC );
		layoutA->addLayout( layoutB );

		const auto updateRGB = [this]
		{
			_red->blockSignals( true );
			_green->blockSignals( true );
			_blue->blockSignals( true );

			const auto color = QColor::fromHsv( _huePicker->hue(), _saturationValuePicker->saturation(), _saturationValuePicker->value() );
			_red->setValue( color.red() );
			_green->setValue( color.green() );
			_blue->setValue( color.blue() );

			_red->blockSignals( false );
			_green->blockSignals( false );
			_blue->blockSignals( false );
		};
		const auto updateHSV = [=]
		{
			const auto color = this->color();

			_huePicker->blockSignals( true );
			_saturationValuePicker->blockSignals( true );

			_huePicker->setHue( color.hue() );
			_saturationValuePicker->setHue( color.hue() );
			_saturationValuePicker->setSaturation( color.saturation() );
			_saturationValuePicker->setValue( color.value() );

			_huePicker->blockSignals( false );
			_saturationValuePicker->blockSignals( false );
			emit colorChanged( this->color() );
		};

		QObject::connect( _huePicker, &HuePicker::hueChanged, [=]
		{
			_saturationValuePicker->setHue( _huePicker->hue() );
			updateRGB();
			emit colorChanged( this->color() );
		} );
		QObject::connect( _saturationValuePicker, &SaturationValuePicker::saturationValueChanged, [=]
		{
			updateRGB();
			emit colorChanged( this->color() );
		} );
		QObject::connect( _red, QOverload<int>::of( &QSpinBox::valueChanged ), updateHSV );
		QObject::connect( _green, QOverload<int>::of( &QSpinBox::valueChanged ), updateHSV );
		QObject::connect( _blue, QOverload<int>::of( &QSpinBox::valueChanged ), updateHSV );
		QObject::connect( _alpha, QOverload<int>::of( &QSpinBox::valueChanged ), [=] { emit colorChanged( this->color() ); } );
	}

	QColor color() const
	{
		return QColor( _red->value(), _green->value(), _blue->value(), _alpha->value() );
	}

signals:
	void colorChanged( QColor );

public slots:
	void setColor( QColor color )
	{
		if( color != this->color() )
		{
			this->blockSignals( true );
			_red->setValue( color.red() );
			_green->setValue( color.green() );
			_blue->setValue( color.blue() );
			_alpha->setValue( color.alpha() );
			this->blockSignals( false );
			emit colorChanged( this->color() );
		}
	}

private:
	HuePicker* _huePicker = nullptr;
	SaturationValuePicker* _saturationValuePicker = nullptr;

	QSpinBox* _red = nullptr;
	QSpinBox* _green = nullptr;
	QSpinBox* _blue = nullptr;
	QSpinBox* _alpha = nullptr;

	bool _handleRGB = true;
};



// Class for displaying a color map
class ColorMap : public QWidget
{
public:
	ColorMap() : QWidget()
	{
		this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );
	}

	void setColorMap( const QImage& colorMap )
	{
		_colorMap = colorMap;
		this->update();
	}
	const QImage& colorMap() const noexcept
	{
		return _colorMap;
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
		painter.drawImage( this->rect(), _colorMap );

		painter.setBrush( Qt::transparent );
		painter.setPen( QColor( 218, 220, 224 ) );
		painter.drawRect( this->rect() );
	}

	QImage _colorMap;
};



// Class for the bar of a parallel coordinates axis
class ParallelCoordinatesAxisBar : public QWidget
{
	Q_OBJECT
public:
	// Enum for the direction of the bar
	enum class Direction { eVertical, eHorizontal };

	// Constructor :)
	ParallelCoordinatesAxisBar( Direction direction, std::vector<vec2d>& intervals, vec2d maximumRange, int precision ) : QWidget(),
		_direction( direction ), _intervals( &intervals ), _maximumRange( maximumRange ), _currentRange( maximumRange ), _precision( precision )
	{
		this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding );
		this->setMouseTracking( true );
	}

	// Setter for the selected intervals
	void setIntervals( std::vector<vec2d>& intervals )
	{
		_intervals = &intervals;
		this->update();
	}

	// Setter for the highlighted value (different from hovered value)
	void setHighlightedValue( double value )
	{
		_highlightedValue = value;
		this->update();
	}

	// Getter for the maximum and current ranges
	vec2d maximumRange() const noexcept
	{
		return _maximumRange;
	}
	vec2d currentRange() const noexcept
	{
		return _currentRange;
	}

	// Getter for mapping the position of the axis end points to the coordinates of another widget
	std::pair<QPoint, QPoint> mapAxesPoints( QWidget* widget ) const
	{
		if( _direction == Direction::eVertical )
		{
			const auto centerX = this->rect().center().x();

			return std::pair<QPoint, QPoint>(
				this->mapTo( widget, QPoint( centerX, _Padding ) ),
				this->mapTo( widget, QPoint( centerX, this->height() - _Padding ) ) );
		}
		else
		{
			const auto centerY = this->rect().center().y();

			return std::pair<QPoint, QPoint>(
				this->mapTo( widget, QPoint( _Padding, centerY ) ),
				this->mapTo( widget, QPoint( this->width() - _Padding, centerY ) ) );
		}
	}

	// Getter for the current intervals
	const std::vector<vec2d>& intervals() const
	{
		return *_intervals;
	}
	std::vector<vec2d> invertedIntervals()
	{
		auto intervals = std::vector<vec2d>();
		auto current = _maximumRange.x;
		for( const auto interval : *_intervals )
		{
			if( current != interval.x ) intervals.push_back( vec2d( current, interval.x ) );
			current = interval.y;
		}
		if( current != _maximumRange.y ) intervals.push_back( vec2d( current, _maximumRange.y ) );
		return intervals;
	}

signals:
	// Signals when the current range changes
	void currentRangeChanged( vec2d );

	// Signals for control settings
	void zoomingEnabledChanged( bool );
	void realtimeEnabledChanged( bool );

	// Signals when the current intervals change (brushing)
	void intervalsChanged( const std::vector<vec2d>& );

	// Signals when the maximum range changes
	void maximumRangeChanged( vec2d );

	// Signals when the precision of displayed values changes
	void precisionChanged( int );

public slots:
	// Setter for the current range
	void setCurrentRange( vec2d range )
	{
		if( _currentRange != range )
		{
			_currentRange = range;
			emit currentRangeChanged( _currentRange );
			this->update();
		}
	}

	// Enables/disabled zooming
	void setZoomingEnabled( bool enabled )
	{
		if( _zoomingEnabled != enabled )
		{
			_zoomingEnabled = enabled;
			emit zoomingEnabledChanged( _zoomingEnabled );
			if( !_zoomingEnabled ) this->setCurrentRange( _maximumRange );
		}
	}

	// Enables/disables real time brushing
	void setRealtimeEnabled( bool enabled )
	{
		if( _realtimeEnabled != enabled )
			emit realtimeEnabledChanged( _realtimeEnabled = enabled );
	}

	// Add, remove an intervals or invert all intervals
	void addInterval( vec2d interval )
	{
		if( interval.x > interval.y ) std::swap( interval.x, interval.y );

		auto insert = std::lower_bound( _intervals->begin(), _intervals->end(), interval, [] ( vec2d first, vec2d second ) { return first.x < second.x; } );
		_intervals->insert( insert, interval );

		for( auto it = _intervals->begin(); true; )
		{
			auto current = it;
			if( ++it == _intervals->end() ) break;

			if( it->x <= current->y )
			{
				current->y = std::max( current->y, it->y );
				_intervals->erase( it );
				it = current;
			};
		}

		emit intervalsChanged( *_intervals );
		this->update();
	}
	void removeInterval( vec2d interval )
	{
		if( interval.x > interval.y ) std::swap( interval.x, interval.y );

		for( auto it = _intervals->begin(); it != _intervals->end(); )
		{
			auto& [lower, upper] = *it;

			if( interval.x <= lower )
			{
				if( interval.y >= upper ) it = _intervals->erase( it );
				else if( interval.y > lower ) lower = interval.y, ++it;
				else ++it;
			}
			else
			{
				if( interval.x < upper )
				{
					if( interval.y >= upper ) upper = interval.x, ++it;
					else
					{
						const auto newUpper = upper;
						upper = interval.x;
						it = _intervals->insert( ++it, vec2d( interval.y, newUpper ) );
					}
				}
				else ++it;
			}
		}

		emit intervalsChanged( *_intervals );
		this->update();
	}
	void invertIntervals()
	{
		*_intervals = this->invertedIntervals();
		emit intervalsChanged( *_intervals );
		this->update();
	}

	// Update the maximum range of values
	void setMaximumRange( vec2d range, bool updateCurrentRange = false )
	{
		if( range != _maximumRange )
		{
			_maximumRange = range;
			emit maximumRangeChanged( _maximumRange );
			if( updateCurrentRange ) this->setCurrentRange( _maximumRange );
			this->update();
		}
	}
	void expandMaximumRange( vec2d range, bool updateCurrentRange = false )
	{
		range.x = std::min( _maximumRange.x, range.x );
		range.y = std::max( _maximumRange.y, range.y );
		this->setMaximumRange( range, updateCurrentRange );
	}

	// Sets the precision of displayed values
	void setPrecision( int precision )
	{
		if( precision != _precision )
		{
			_precision = precision;
			emit precisionChanged( _precision );
			this->update();
		}
	}

private:
	// Size hints for the widget
	QSize sizeHint() const override
	{
		if( _direction == Direction::eVertical )
		{
			const auto lowerWidth = this->fontMetrics().width( QString::number( _maximumRange.x, 'f', _precision ) );
			const auto upperWidth = this->fontMetrics().width( QString::number( _maximumRange.y, 'f', _precision ) );
			return QSize( _Padding + 2 * std::max( lowerWidth, upperWidth ), 100 );
		}
		else
		{
			const auto height = this->fontMetrics().height();
			return QSize( 100, _Padding + 2 * height );
		}
	}

	// Render the axis bar, intervals etc.
	void paintEvent( QPaintEvent* event ) override
	{
		// Draw the entire bar
		auto painter = QPainter( this );
		painter.setRenderHint( QPainter::Antialiasing, true );

		const auto colorSelected = QColor( 26, 115, 232 );
		const auto colorUnselected = QColor( 200, 222, 249 );

		int center;
		QRect rect;
		if( _direction == Direction::eVertical )
		{
			center = this->rect().center().x();
			rect = QRect( center - 1, _Padding, 2, this->height() - 2 * _Padding );
		}
		else
		{
			center = this->rect().center().y();
			rect = QRect( _Padding, center - 1, this->width() - 2 * _Padding, 2 );
		}
		painter.fillRect( rect, colorUnselected );

		// Helper function for drawing intervals
		const auto drawIntervalPoints = [&] ( int first, int second )
		{
			if( _direction == Direction::eVertical ) painter.fillRect( QRect( center - 1, first, 2, second - first ), colorSelected );
			else painter.fillRect( QRect( first, center - 1, second - first, 2 ), colorSelected );
		};
		const auto drawIntervalValues = [&] ( vec2d interval )
		{
			if( interval.x > _currentRange.y || interval.y < _currentRange.x ) return;
			const auto begin = this->valueToPoint( interval.x );
			const auto end = this->valueToPoint( interval.y );

			if( _direction == Direction::eVertical )
			{
				const auto first = std::clamp( begin.y(), _Padding, this->height() - _Padding );
				const auto second = std::clamp( end.y(), _Padding, this->height() - _Padding );
				drawIntervalPoints( first, second );
			}
			else
			{
				const auto first = std::clamp( begin.x(), _Padding, this->width() - _Padding );
				const auto second = std::clamp( end.x(), _Padding, this->width() - _Padding );
				drawIntervalPoints( first, second );
			}
		};

		// Draw selected intervals
		for( const auto interval : *_intervals ) drawIntervalValues( interval );

		// Helper function for drawing points with text on the axis bar
		const auto drawPointWithText = [&] ( double value, QPoint point, bool highlight, bool extra )
		{
			if( highlight )
			{
				painter.setPen( colorSelected );
				painter.setBrush( colorUnselected );
				painter.drawEllipse( point, 3, 3 );
			}
			else
			{
				painter.setPen( Qt::transparent );
				painter.setBrush( colorSelected );
				painter.drawEllipse( point, 5, 5 );
			}

			const auto text = QString::number( value, 'f', _precision );
			const auto h = this->fontMetrics().height();

			if( _direction == Direction::eVertical )
			{
				auto rect = QRect( point.x(), point.y() - 5 - h, this->width(), h );
				if( rect.top() < 0 ) rect.moveTop( point.y() + 5 );

				auto halignment = extra ? Qt::AlignRight : Qt::AlignLeft;
				if( halignment == Qt::AlignRight ) rect.moveRight( center - 5 );
				else if( halignment == Qt::AlignLeft ) rect.moveLeft( center + 5 );

				painter.setPen( "#202124" );
				painter.setBrush( Qt::transparent );
				painter.drawText( rect, Qt::AlignVCenter | halignment, text );
			}
			else
			{
				const auto w = this->fontMetrics().width( text );
				auto rect = QRect( point.x(), point.y() - 5 - h, w, h );
				if( rect.right() > this->width() ) rect.moveRight( point.x() );

				auto valignment = extra ? Qt::AlignTop : Qt::AlignBottom;
				if( valignment == Qt::AlignTop ) rect.moveTop( center + 5 );
				else if( valignment == Qt::AlignBottom ) rect.moveBottom( center - 5 );

				painter.setPen( "#202124" );
				painter.setBrush( Qt::transparent );
				painter.drawText( rect, Qt::AlignHCenter | valignment, text );
			}
		};

		// Draw highlighted value when not currently brushing
		if( _highlightedValue != std::numeric_limits<double>::infinity() && _beginValue == std::numeric_limits<double>::infinity() )
		{
			const auto highlightedPoint = this->valueToPoint( _highlightedValue );
			drawPointWithText( _highlightedValue, highlightedPoint, true, false );
		}

		// Draw hovered point
		if( _hoveredValue != std::numeric_limits<double>::infinity() )
		{
			const auto hoveredPoint = this->valueToPoint( _hoveredValue );

			if( _beginValue != std::numeric_limits<double>::infinity() )
			{
				const auto beginPoint = this->valueToPoint( _beginValue );
				if( _direction == Direction::eVertical ) drawIntervalPoints( beginPoint.y(), hoveredPoint.y() );
				else drawIntervalPoints( beginPoint.x(), hoveredPoint.x() );
				drawPointWithText( _beginValue, beginPoint, false, true );
			}
			drawPointWithText( _hoveredValue, hoveredPoint, false, false );
		}
	}

	// Handle mouse move events
	void mouseMoveEvent( QMouseEvent* event ) override
	{
		if( event->buttons() == Qt::NoButton ) this->updateHoveredPoint( event->pos() );
		else if( _mode != IntervalMode::eNone )
		{
			_hoveredValue = this->pointToValue( event->pos() );

			// Apply brushing in real time if enabled
			if( _realtimeEnabled && ( _mode == IntervalMode::eAdding || _mode == IntervalMode::eRemoving ) )
			{
				*_intervals = _previousIntervals;
				if( _mode == IntervalMode::eAdding ) this->addInterval( vec2d( _beginValue, _hoveredValue ) );
				else if( _mode == IntervalMode::eRemoving ) this->removeInterval( vec2d( _beginValue, _hoveredValue ) );
			}

			this->update();
		}
	}

	// Reset hovered value when mouse leaves
	void leaveEvent( QEvent* event ) override
	{
		_hoveredValue = std::numeric_limits<double>::infinity();
		this->update();
	}

	// Handle mouse button press events
	void mousePressEvent( QMouseEvent* event ) override
	{
		if( event->button() == Qt::LeftButton || event->button() == Qt::RightButton || event->button() == Qt::MiddleButton )
		{
			// Start drawing an interval
			if( _hoveredValue != std::numeric_limits<double>::infinity() )
			{
				_beginValue = _hoveredValue;
				_mode = event->button() == Qt::LeftButton ? IntervalMode::eAdding
					: event->button() == Qt::RightButton ? IntervalMode::eRemoving
					: IntervalMode::eZooming;

				if( _realtimeEnabled && ( _mode == IntervalMode::eAdding || _mode == IntervalMode::eRemoving ) ) _previousIntervals = *_intervals;
			}
		}
	}

	// Handle mouse button release events
	void mouseReleaseEvent( QMouseEvent* event ) override
	{
		if( _mode != IntervalMode::eNone )
		{
			// Add, remove or zoom to the selected interval
			if( _mode == IntervalMode::eAdding ) this->addInterval( vec2d( _beginValue, _hoveredValue ) );
			else if( _mode == IntervalMode::eRemoving ) this->removeInterval( vec2d( _beginValue, _hoveredValue ) );
			else if( _mode == IntervalMode::eZooming )
			{
				const auto [min, max] = std::minmax( _beginValue, _hoveredValue );
				emit currentRangeChanged( _currentRange = vec2d( min, max ) );
				this->updateHoveredPoint( event->pos() );
			}

			_beginValue = std::numeric_limits<double>::infinity();
			_mode = IntervalMode::eNone;
			if( _realtimeEnabled ) _previousIntervals.clear();
			this->update();
		}
	}

	// Handle mouse wheel events
	void wheelEvent( QWheelEvent* event ) override
	{
		if( !_zoomingEnabled || _hoveredValue == std::numeric_limits<double>::infinity() ) return;

		const auto maxdiff = _maximumRange.y - _maximumRange.x;
		const auto step = maxdiff * 0.05;

		if( event->modifiers() == Qt::ControlModifier )
		{
			event->accept();

			// Zoom in/out
			auto diff = std::clamp( ( _currentRange.y - _currentRange.x ) + ( event->delta() > 0 ? -step : step ), step, maxdiff );
			auto center = _currentRange.sum() / 2.0;
			auto left = center - diff / 2.0;
			auto right = center + diff / 2.0;

			if( left < _maximumRange.x ) left = _maximumRange.x, right = left + diff;
			if( right > _maximumRange.y ) right = _maximumRange.y, left = right - diff;

			_currentRange = vec2d( left, right );
			emit currentRangeChanged( _currentRange );
			this->updateHoveredPoint( event->pos() );
			this->update();
		}
		else if( event->modifiers() == Qt::ShiftModifier )
		{
			event->accept();

			// Shift the current range up/down
			const auto diff = _currentRange.y - _currentRange.x;
			const auto dir = event->delta() > 0 ? step : -step;
			auto left = _currentRange.x + dir;
			auto right = _currentRange.y + dir;

			if( left < _maximumRange.x ) left = _maximumRange.x, right = left + diff;
			if( right > _maximumRange.y ) right = _maximumRange.y, left = right - diff;

			_currentRange = vec2d( left, right );
			emit currentRangeChanged( _currentRange );
			this->updateHoveredPoint( event->pos() );
			this->update();
		}
	}

	// Helper function to map a screen point to a value and vice versa
	double pointToValue( QPoint point )
	{
		if( _direction == Direction::eVertical )
		{
			const auto y = 1.0 - std::clamp( static_cast<double>( point.y() - _Padding ) / ( this->height() - 2 * _Padding ), 0.0, 1.0 );
			return _currentRange.x + y * ( _currentRange.y - _currentRange.x );
		}
		else
		{
			const auto x = std::clamp( static_cast<double>( point.x() - _Padding ) / ( this->width() - 2 * _Padding ), 0.0, 1.0 );
			return _currentRange.x + x * ( _currentRange.y - _currentRange.x );
		}
	}
	QPoint valueToPoint( double value )
	{
		if( _direction == Direction::eVertical )
		{
			const auto y = 1.0 - ( value - _currentRange.x ) / ( _currentRange.y - _currentRange.x );
			return QPoint( this->rect().center().x(), _Padding + y * ( this->height() - 2 * _Padding ) );
		}
		else
		{
			const auto x = ( value - _currentRange.x ) / ( _currentRange.y - _currentRange.x );
			return QPoint( _Padding + x * ( this->width() - 2 * _Padding ), this->rect().center().y() );
		}
	}

	// Update the currently hovered point
	void updateHoveredPoint( QPoint cursor )
	{
		const auto distance = ( _direction == Direction::eVertical ) ? std::abs( this->rect().center().x() - cursor.x() ) : std::abs( this->rect().center().y() - cursor.y() );

		if( distance < 5 )
		{
			_hoveredValue = this->pointToValue( cursor );
			this->update();
		}
		else if( _hoveredValue != std::numeric_limits<double>::infinity() )
		{
			_hoveredValue = std::numeric_limits<double>::infinity();
			this->update();
		}
	}

	Direction _direction;
	vec2d _maximumRange, _currentRange;
	int _precision = 0;
	bool _zoomingEnabled = true;
	bool _realtimeEnabled = false;

	double _hoveredValue = std::numeric_limits<double>::infinity();
	double _beginValue = std::numeric_limits<double>::infinity();
	double _highlightedValue = std::numeric_limits<double>::infinity();
	enum class IntervalMode { eNone, eAdding, eRemoving, eZooming } _mode { IntervalMode::eNone };

	std::vector<vec2d>* _intervals = nullptr;
	std::vector<vec2d> _previousIntervals;

	static constexpr int32_t _Padding = 10;
};