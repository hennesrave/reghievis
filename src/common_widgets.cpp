#include "common_widgets.hpp"


CheckBox::CheckBox( bool checked ) : QWidget(), _checked( checked ), _position( checked ? 18 : 0 ), _animation( new QPropertyAnimation( this, "position" ) )
{
	this->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed );

	_animation->setDuration( 100 );

	QObject::connect( _animation, &QPropertyAnimation::valueChanged, [this] { this->update(); } );
}

void CheckBox::setChecked( bool checked )
{
	if( checked != _checked )
	{
		_checked = checked;
		_position = _checked? 18 : 0;

		emit stateChanged( _checked );
		this->update();
	}
}
bool CheckBox::checked() const noexcept
{
	return _checked;
}

QSize CheckBox::sizeHint() const
{
	return QSize( 34, 16 );
}

void CheckBox::paintEvent( QPaintEvent* event )
{
	auto painter = QPainter( this );
	painter.setRenderHint( QPainter::Antialiasing, true );

	painter.setPen( Qt::transparent );
	painter.setBrush( _checked ? QColor( 141, 185, 244 ) : QColor( 189, 193, 198 ) );
	painter.drawRoundedRect( QRect( 3, 2, 28, 12 ), 6, 6 );

	painter.setBrush( _checked ? QColor( 26, 115, 232 ) : QColor( 255, 255, 255 ) );
	if( !_checked ) painter.setPen( QColor( 208, 208, 208 ) );
	painter.drawEllipse( QRect( QPoint( _position, 0 ), QSize( 16, 16 ) ) );
}
void CheckBox::mouseReleaseEvent( QMouseEvent* event )
{
	if( event->button() == Qt::LeftButton )
	{
		if( _checked = !_checked )
		{
			_animation->setStartValue( 0 );
			_animation->setEndValue( 18 );
			_animation->start();
		}
		else
		{
			_animation->setStartValue( 18 );
			_animation->setEndValue( 0 );
			_animation->start();
		}

		emit stateChanged( _checked );
	}
}





Slider::Slider( bool integerSteps, double minimum, double maximum, double value ) : QWidget(),
_integerSteps( integerSteps ), _minimum( minimum ), _maximum( maximum ), _value( value )
{
	this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );
}

double Slider::value() const noexcept
{
	return _value;
}
double Slider::minimum() const noexcept
{
	return _minimum;
}
double Slider::maximum() const noexcept
{
	return _maximum;
}
vec2d Slider::range() const noexcept
{
	return vec2d( _minimum, _maximum );
}

void Slider::setValue( double value )
{
	if( value != _value )
	{
		_value = value;
		this->update();
		emit valueChanged( _value );
	}
}
void Slider::setMinimum( double minimum )
{
	_minimum = minimum;
	if( _maximum < _minimum )
		_maximum = _minimum;

	if( _value < _minimum )
		this->setValue( _minimum );
}
void Slider::setMaximum( double maximum )
{
	_maximum = maximum;
	if( _minimum > _maximum )
		_minimum = _maximum;

	if( _value > _maximum )
		this->setValue( _maximum );
}
void Slider::setRange( double minimum, double maximum )
{
	_minimum = minimum;
	_maximum = maximum;

	if( _value < _minimum ) this->setValue( _minimum );
	else if( _value > _maximum ) this->setValue( _maximum );
}

QSize Slider::sizeHint() const
{
	return QSize( 2 * _Padding + 100, 2 * _Padding + 10 );
}
void Slider::paintEvent( QPaintEvent* event )
{
	auto painter = QPainter( this );
	painter.setRenderHint( QPainter::Antialiasing, true );

	const auto range = _maximum - _minimum;
	const auto x = range ? static_cast<double>( _value - _minimum ) / range : 0.0;

	const auto left = _Padding;
	const auto right = this->width() - _Padding;
	const auto middle = std::round( left + x * ( right - left ) );

	painter.fillRect( QRect( _Padding, _Padding + 4, middle - left, 2 ), QColor( 26, 115, 232 ) );
	painter.fillRect( QRect( middle, _Padding + 4, right - middle, 2 ), QColor( 200, 222, 249 ) );

	painter.setPen( Qt::transparent );
	painter.setBrush( QColor( 26, 115, 232 ) );
	painter.drawEllipse( QRect( middle - 5, _Padding, 10, 10 ) );
}

void Slider::mouseMoveEvent( QMouseEvent* event )
{
	if( event->buttons() == Qt::LeftButton )
		this->updateValue( event->pos() );
}
void Slider::mousePressEvent( QMouseEvent* event )
{
	if( event->button() == Qt::LeftButton )
		this->updateValue( event->pos() );
}

void Slider::updateValue( QPoint cursor )
{
	const auto x = std::clamp( static_cast<double>( cursor.x() - _Padding ) / ( this->width() - 2 * _Padding ), 0.0, 1.0 );
	auto value = _minimum + x * ( _maximum - _minimum );
	if( _integerSteps ) value = std::round( value );

	this->setValue( value );
}





RangeSlider::RangeSlider( bool integerSteps, double minimum, double maximum, double lower, double upper )
	: QWidget(), _integerSteps( integerSteps ), _minimum( minimum ), _maximum( maximum ), _lower( lower ), _upper( upper )
{
	this->setSizePolicy( QSizePolicy::MinimumExpanding, QSizePolicy::Fixed );
}

double RangeSlider::lower() const noexcept
{
	return _lower;
}
double RangeSlider::upper() const noexcept
{
	return _upper;
}
vec2d RangeSlider::values() const noexcept
{
	return vec2d( _lower, _upper );
}

double RangeSlider::minimum() const noexcept
{
	return _minimum;
}
double RangeSlider::maximum() const noexcept
{
	return _maximum;
}
vec2d RangeSlider::range() const noexcept
{
	return vec2d( _minimum, _maximum );
}

void RangeSlider::setLower( double lower )
{
	if( lower != _lower )
	{
		_lower = lower;
		this->update();
		emit valuesChanged( _lower, _upper );
	}
}
void RangeSlider::setUpper( double upper )
{
	if( upper != _upper )
	{
		_upper = upper;
		this->update();
		emit valuesChanged( _lower, _upper );
	}
}
void RangeSlider::setValues( double lower, double upper )
{
	if( lower != _lower || upper != _upper )
	{
		_lower = lower;
		_upper = upper;
		this->update();
		emit valuesChanged( _lower, _upper );
	}
}

void RangeSlider::setMinimum( double minimum )
{
	_minimum = minimum;
	if( _maximum < _minimum )
		_maximum = _minimum;

	bool signal = false;
	if( _upper < _minimum ) _lower = _minimum, _upper = _minimum, signal = true;
	else if( _lower < _minimum ) _lower = _minimum, signal = true;
	if( signal ) emit valuesChanged( _lower, _upper );
}
void RangeSlider::setMaximum( double maximum )
{
	_maximum = maximum;
	if( _minimum > _maximum )
		_minimum = _maximum;

	bool signal = false;
	if( _lower > _maximum ) _lower = _maximum, _upper = _maximum, signal = true;
	else if( _upper > _maximum ) _lower = _maximum, signal = true;
	if( signal ) emit valuesChanged( _lower, _upper );
}
void RangeSlider::setRange( double minimum, double maximum )
{
	_minimum = minimum;
	_maximum = maximum;

	bool signal = false;
	const auto lower = std::clamp( _lower, _minimum, _maximum );
	const auto upper = std::clamp( _upper, _minimum, _maximum );
	if( _lower != lower ) _lower = lower, signal = true;
	if( _upper != upper ) _upper = upper, signal = true;
	if( signal ) emit valuesChanged( _lower, _upper );
}

QSize RangeSlider::sizeHint() const
{
	return QSize( 2 * _Padding + 100, 2 * _Padding + 10 );
}
void RangeSlider::paintEvent( QPaintEvent* event )
{
	auto painter = QPainter( this );
	painter.setRenderHint( QPainter::Antialiasing, true );

	const auto range = _maximum - _minimum;
	const auto xlower = range ? static_cast<double>( _lower - _minimum ) / range : 0.0;
	const auto xupper = range ? static_cast<double>( _upper - _minimum ) / range : 0.0;

	const auto left = _Padding;
	const auto right = this->width() - _Padding;
	const auto lower = std::round( left + xlower * ( right - left ) );
	const auto upper = std::round( left + xupper * ( right - left ) );

	painter.fillRect( QRect( _Padding, _Padding + 4, lower - left, 2 ), QColor( 200, 222, 249 ) );
	painter.fillRect( QRect( upper, _Padding + 4, right - upper, 2 ), QColor( 200, 222, 249 ) );
	painter.fillRect( QRect( lower, _Padding + 4, upper - lower, 2 ), QColor( 26, 115, 232 ) );

	painter.setPen( Qt::transparent );
	painter.setBrush( QColor( 26, 115, 232 ) );
	painter.drawChord( QRect( lower - 5, _Padding, 10, 10 ), 16 * 90, 16 * 180 );
	painter.drawChord( QRect( upper - 5, _Padding, 10, 10 ), -16 * 90, 16 * 180 );
}

void RangeSlider::mouseMoveEvent( QMouseEvent* event )
{
	if( event->buttons() == Qt::LeftButton )
	{
		const auto x = std::clamp( static_cast<double>( event->pos().x() - _Padding ) / ( this->width() - 2 * _Padding ), 0.0, 1.0 );
		auto value = _minimum + x * ( _maximum - _minimum );
		if( _integerSteps ) value = std::round( value );

		if( _moving == eLower )
		{
			_lower = value;
			if( _lower > _upper ) _upper = _lower;
			_moving = eLower;
		}
		else
		{
			_upper = value;
			if( _upper < _lower ) _lower = _upper;
			_moving = eUpper;
		}

		emit valuesChanged( _lower, _upper );
		this->update();
	}
}
void RangeSlider::mousePressEvent( QMouseEvent* event )
{
	if( event->button() == Qt::LeftButton )
	{
		const auto x = std::clamp( static_cast<double>( event->pos().x() - _Padding ) / ( this->width() - 2 * _Padding ), 0.0, 1.0 );
		auto value = _minimum + x * ( _maximum - _minimum );

		if( std::abs( value - _lower ) < std::abs( value - _upper ) )
		{
			if( _integerSteps ) value = std::round( value );
			_lower = value;
			_moving = eLower;
		}
		else
		{
			if( _integerSteps ) value = std::round( value );
			_upper = value;
			_moving = eUpper;
		}

		emit valuesChanged( _lower, _upper );
		this->update();
	}
}





NumberWidget::NumberWidget( double minimum, double maximum, double value, double stepSize, int precision ) : QWidget()
{
	auto layout = new QHBoxLayout( this );
	layout->setContentsMargins( 0, 0, 0, 0 );
	layout->setSpacing( 5 );

	_spinbox = new QDoubleSpinBox();
	_spinbox->setButtonSymbols( QAbstractSpinBox::NoButtons );
	_spinbox->setRange( minimum, maximum );
	_spinbox->setValue( value );
	_spinbox->setSingleStep( stepSize );
	_spinbox->setDecimals( precision );
	layout->addWidget( _spinbox );

	_slider = new Slider( !precision, minimum, maximum, value );
	layout->addWidget( _slider );

	QObject::connect( _spinbox, QOverload<double>::of( &QDoubleSpinBox::valueChanged ), _slider, &Slider::setValue );
	QObject::connect( _slider, &Slider::valueChanged, _spinbox, &QDoubleSpinBox::setValue );
	QObject::connect( _slider, &Slider::valueChanged, this, &NumberWidget::valueChanged );
}
NumberWidget::NumberWidget( double minimum, double maximum, double value ) : NumberWidget( minimum, maximum, value, 1.0, 0 )
{}

double NumberWidget::value() const noexcept
{
	return _slider->value();
}
double NumberWidget::minimum() const noexcept
{
	return _slider->minimum();
}
double NumberWidget::maximum() const noexcept
{
	return _slider->maximum();
}
vec2d NumberWidget::range() const noexcept
{
	return _slider->range();
}

void NumberWidget::setStepSize( double stepSize )
{
	_spinbox->setSingleStep( stepSize );
}
void NumberWidget::setPrecision( int precision )
{
	_spinbox->setDecimals( precision );
}

void NumberWidget::setValue( double value )
{
	_slider->setValue( value );
}
void NumberWidget::setMinimum( double minimum )
{
	_spinbox->setMinimum( minimum );
	_slider->setMinimum( minimum );
}
void NumberWidget::setMaximum( double maximum )
{
	_spinbox->setMaximum( maximum );
	_slider->setMaximum( maximum );
}
void NumberWidget::setRange( double minimum, double maximum )
{
	_spinbox->setRange( minimum, maximum );
	_slider->setRange( minimum, maximum );
}