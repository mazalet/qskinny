/******************************************************************************
 * QSkinny - Copyright (C) 2016 Uwe Rathmann
 * This file may be used under the terms of the QSkinny License, Version 1.0
 *****************************************************************************/

#include "QskSkinnable.h"

#include "QskAnimationHint.h"
#include "QskAspect.h"
#include "QskColorFilter.h"
#include "QskControl.h"
#include "QskHintAnimator.h"
#include "QskMargins.h"
#include "QskSetup.h"
#include "QskSkin.h"
#include "QskSkinHintTable.h"
#include "QskSkinTransition.h"
#include "QskSkinlet.h"

#include <qfont.h>

#define DEBUG_MAP 0
#define DEBUG_ANIMATOR 0
#define DEBUG_STATE 0

static inline bool qskIsControl( const QskSkinnable* skinnable )
{
#if QT_VERSION >= QT_VERSION_CHECK( 5, 7, 0 )
    return skinnable->metaObject()->inherits( &QskControl::staticMetaObject );
#else
    for ( auto mo = skinnable->metaObject();
        mo != nullptr; mo = mo->superClass() )
    {
        if ( mo == &QskControl::staticMetaObject )
            return true;
    }

    return false;
#endif
}

static inline bool qskCompareResolvedStates(
    QskAspect::Aspect& aspect1, QskAspect::Aspect& aspect2,
    const QskSkinHintTable& table )
{
    if ( !table.hasStates() )
        return false;

    const QskAspect::Aspect a1 = aspect1;
    const QskAspect::Aspect a2 = aspect2;

    Q_FOREVER
    {
        const auto s1 = aspect1.topState();
        const auto s2 = aspect2.topState();

        if ( s1 > s2 )
        {
            if ( table.hasHint( aspect1 ) )
                return false;

            aspect1.clearState( s1 );
        }
        else if ( s2 > s1 )
        {
            if ( table.hasHint( aspect2 ) )
                return false;

            aspect2.clearState( s2 );
        }
        else
        {
            if ( aspect1 == aspect2 )
            {
                if ( table.hasHint( aspect1 ) )
                    return true;

                if ( s1 == 0 )
                {
                    if ( aspect1.placement() == QskAspect::NoPlacement )
                        return true;

                    // clear the placement bits and restart with the initial state
                    aspect1 = a1;
                    aspect1.setPlacement( QskAspect::NoPlacement );

                    aspect2 = a2;
                    aspect2.setPlacement( QskAspect::NoPlacement );
                }
            }
            else
            {
                if ( table.hasHint( aspect1 ) || table.hasHint( aspect2 ) )
                    return false;
            }

            aspect1.clearState( s1 );
            aspect2.clearState( s2 );
        }
    }
}

static inline QVariant qskTypedNullValue( const QVariant& value )
{
#if QT_VERSION >= QT_VERSION_CHECK( 6, 0, 0 )
    const auto vType = static_cast< QMetaType >( value.userType() );
#else
    const auto vType = value.userType();
#endif

    return QVariant( vType, nullptr );
}

class QskSkinnable::PrivateData
{
  public:
    PrivateData()
        : skinlet( nullptr )
        , skinState( QskAspect::NoState )
        , hasLocalSkinlet( false )
    {
    }

    ~PrivateData()
    {
        if ( hasLocalSkinlet )
        {
            if ( skinlet && skinlet->isOwnedBySkinnable() )
                delete skinlet;
        }
    }

    QskSkinHintTable hintTable;
    QskHintAnimatorTable animators;

    const QskSkinlet* skinlet;

    QskAspect::State skinState;
    bool hasLocalSkinlet : 1;
};

QskSkinnable::QskSkinnable()
    : m_data( new PrivateData() )
{
}

QskSkinnable::~QskSkinnable()
{
}

void QskSkinnable::setSkinlet( const QskSkinlet* skinlet )
{
    if ( skinlet == m_data->skinlet )
    {
        if ( skinlet )
        {
            // now we don't depend on global skin changes anymore
            m_data->hasLocalSkinlet = true;
        }
        return;
    }

    if ( m_data->skinlet && m_data->skinlet->isOwnedBySkinnable() )
        delete m_data->skinlet;

    m_data->skinlet = skinlet;
    m_data->hasLocalSkinlet = ( skinlet != nullptr );

    owningControl()->update();
}

const QskSkinlet* QskSkinnable::skinlet() const
{
    return m_data->hasLocalSkinlet ? m_data->skinlet : nullptr;
}

const QskSkinlet* QskSkinnable::effectiveSkinlet() const
{
    if ( m_data->skinlet == nullptr )
    {
        m_data->skinlet = qskSetup->skin()->skinlet( this );
        m_data->hasLocalSkinlet = false;
    }

    return m_data->skinlet;
}

QskSkinHintTable& QskSkinnable::hintTable()
{
    return m_data->hintTable;
}

const QskSkinHintTable& QskSkinnable::hintTable() const
{
    return m_data->hintTable;
}

void QskSkinnable::setFlagHint( QskAspect::Aspect aspect, int flag )
{
    m_data->hintTable.setHint( aspect, QVariant( flag ) );
}

int QskSkinnable::flagHint( QskAspect::Aspect aspect ) const
{
    return effectiveHint( aspect ).toInt();
}

void QskSkinnable::setColor( QskAspect::Aspect aspect, const QColor& color )
{
    m_data->hintTable.setColor( aspect, color );
}

void QskSkinnable::setColor( QskAspect::Aspect aspect, Qt::GlobalColor color )
{
    m_data->hintTable.setColor( aspect, color );
}

void QskSkinnable::setColor( QskAspect::Aspect aspect, QRgb rgb )
{
    m_data->hintTable.setColor( aspect, rgb );
}

QColor QskSkinnable::color( QskAspect::Aspect aspect, QskSkinHintStatus* status ) const
{
    return effectiveHint( aspect | QskAspect::Color, status ).value< QColor >();
}

void QskSkinnable::setMetric( QskAspect::Aspect aspect, qreal metric )
{
    m_data->hintTable.setMetric( aspect, metric );
}

qreal QskSkinnable::metric( QskAspect::Aspect aspect, QskSkinHintStatus* status ) const
{
    return effectiveHint( aspect | QskAspect::Metric, status ).toReal();
}

void QskSkinnable::setMarginsHint( QskAspect::Aspect aspect, qreal margins )
{
    m_data->hintTable.setMargins( aspect, QskMargins( margins ) );
}

void QskSkinnable::setMarginsHint( QskAspect::Aspect aspect, const QMarginsF& margins )
{
    m_data->hintTable.setMargins( aspect, margins );
}

QMarginsF QskSkinnable::marginsHint(
    QskAspect::Aspect aspect, QskSkinHintStatus* status ) const
{
    return effectiveHint( aspect | QskAspect::Metric, status ).value< QskMargins >();
}

void QskSkinnable::setGradientHint(
    QskAspect::Aspect aspect, const QskGradient& gradient )
{
    m_data->hintTable.setGradient( aspect, gradient );
}

QskGradient QskSkinnable::gradientHint(
    QskAspect::Aspect aspect, QskSkinHintStatus* status ) const
{
    return effectiveHint( aspect | QskAspect::Color, status ).value< QskGradient >();
}

void QskSkinnable::setBoxShapeHint(
    QskAspect::Aspect aspect, const QskBoxShapeMetrics& shape )
{
    m_data->hintTable.setBoxShape( aspect, shape );
}

QskBoxShapeMetrics QskSkinnable::boxShapeHint(
    QskAspect::Aspect aspect, QskSkinHintStatus* status ) const
{
    using namespace QskAspect;
    return effectiveHint( aspect | Metric | Shape, status ).value< QskBoxShapeMetrics >();
}

void QskSkinnable::setBoxBorderMetricsHint(
    QskAspect::Aspect aspect, const QskBoxBorderMetrics& border )
{
    m_data->hintTable.setBoxBorder( aspect, border );
}

QskBoxBorderMetrics QskSkinnable::boxBorderMetricsHint(
    QskAspect::Aspect aspect, QskSkinHintStatus* status ) const
{
    using namespace QskAspect;
    return effectiveHint( aspect | Metric | Border, status ).value< QskBoxBorderMetrics >();
}

void QskSkinnable::setBoxBorderColorsHint(
    QskAspect::Aspect aspect, const QskBoxBorderColors& colors )
{
    m_data->hintTable.setBoxBorderColors( aspect, colors );
}

QskBoxBorderColors QskSkinnable::boxBorderColorsHint(
    QskAspect::Aspect aspect, QskSkinHintStatus* status ) const
{
    using namespace QskAspect;
    return effectiveHint( aspect | Color | Border, status ).value< QskBoxBorderColors >();
}

void QskSkinnable::setIntervalHint(
    QskAspect::Aspect aspect, const QskIntervalF& interval )
{
    m_data->hintTable.setInterval( aspect, interval );
}

QskIntervalF QskSkinnable::intervalHint(
    QskAspect::Aspect aspect, QskSkinHintStatus* status ) const
{
    return effectiveHint( aspect | QskAspect::Metric, status ).value< QskIntervalF >();
}

void QskSkinnable::setFontRole( QskAspect::Aspect aspect, int role )
{
    m_data->hintTable.setFontRole( aspect, role );
}

int QskSkinnable::fontRole(
    QskAspect::Aspect aspect, QskSkinHintStatus* status ) const
{
    return effectiveHint( aspect | QskAspect::FontRole, status ).toInt();
}

QFont QskSkinnable::effectiveFont( QskAspect::Aspect aspect ) const
{
    return effectiveSkin()->font( fontRole( aspect ) );
}

void QskSkinnable::setGraphicRole( QskAspect::Aspect aspect, int role )
{
    m_data->hintTable.setGraphicRole( aspect, role );
}

int QskSkinnable::graphicRole(
    QskAspect::Aspect aspect, QskSkinHintStatus* status ) const
{
    return effectiveHint( aspect | QskAspect::GraphicRole, status ).toInt();
}

QskColorFilter QskSkinnable::effectiveGraphicFilter(
    QskAspect::Aspect aspect ) const
{
    aspect.setSubControl( effectiveSubcontrol( aspect.subControl() ) );
    aspect.setPlacement( effectivePlacement() );
    aspect = aspect | QskAspect::GraphicRole;

    QskSkinHintStatus status;

    const QVariant hint = storedHint( aspect | skinState(), &status );
    if ( status.isValid() )
    {
        // we need to know about how the aspect gets resolved
        // before checking for animators

        aspect.setSubControl( status.aspect.subControl() );
    }

    if ( !aspect.isAnimator() )
    {
        QVariant v = animatedValue( aspect, nullptr );
        if ( v.canConvert< QskColorFilter >() )
            return v.value< QskColorFilter >();

        if ( auto control = owningControl() )
        {
            v = QskSkinTransition::animatedGraphicFilter(
                    control->window(), hint.toInt() );

            if ( v.canConvert< QskColorFilter >() )
            {
                /*
                    As it is hard to find out which controls depend
                    on the animated graphic filters we reschedule
                    our updates here.
                 */
                control->update();
                return v.value< QskColorFilter >();
            }
        }
    }

    return effectiveSkin()->graphicFilter( hint.toInt() );
}

void QskSkinnable::setAnimation(
    QskAspect::Aspect aspect, QskAnimationHint animation )
{
    m_data->hintTable.setAnimation( aspect, animation );
}

QskAnimationHint QskSkinnable::animation(
    QskAspect::Aspect aspect, QskSkinHintStatus* status ) const
{
    aspect.setAnimator( true );
    return effectiveHint( aspect, status ).value< QskAnimationHint >();
}

QskAnimationHint QskSkinnable::effectiveAnimation(
    QskAspect::Type type, QskAspect::Subcontrol subControl,
    QskAspect::State state, QskSkinHintStatus* status ) const
{
    QskAspect::Aspect aspect = subControl | type | state;
    aspect.setAnimator( true );

    QskAnimationHint hint;

    const auto a = m_data->hintTable.resolvedAnimator( aspect, hint );
    if ( a.isAnimator() )
    {
        if ( status )
        {
            status->source = QskSkinHintStatus::Skinnable;
            status->aspect = a;
        }

        return hint;
    }

    if ( auto skin = effectiveSkin() )
    {
        const auto a = skin->hintTable().resolvedAnimator( aspect, hint );
        if ( a.isAnimator() )
        {
            if ( status )
            {
                status->source = QskSkinHintStatus::Skin;
                status->aspect = a;
            }

            return hint;
        }
    }

    if ( status )
    {
        status->source = QskSkinHintStatus::NoSource;
        status->aspect = QskAspect::Aspect();
    }

    return hint;
}

bool QskSkinnable::resetHint( QskAspect::Aspect aspect )
{
    if ( !m_data->hintTable.hasHint( aspect ) )
        return false;

    /*
        To be able to indicate, when the resolved value has changed
        we retrieve the value before and after removing the hint from
        the local table. An implementation with less lookups
        should be possible, but as reset is a low frequently called
        operation, we prefer to keep the implementation simple.
     */

    auto a = aspect;
    a.setSubControl( effectiveSubcontrol( a.subControl() ) );
    a.setPlacement( effectivePlacement() );

    if ( a.state() == QskAspect::NoState )
        a = a | skinState();

    const auto oldHint = storedHint( a );

    m_data->hintTable.removeHint( aspect );

    return oldHint != storedHint( a );
}

QVariant QskSkinnable::effectiveHint(
    QskAspect::Aspect aspect, QskSkinHintStatus* status ) const
{
    aspect.setSubControl( effectiveSubcontrol( aspect.subControl() ) );
    aspect.setPlacement( effectivePlacement() );

    if ( aspect.isAnimator() )
        return storedHint( aspect, status );

    const QVariant v = animatedValue( aspect, status );
    if ( v.isValid() )
        return v;

    if ( aspect.state() == QskAspect::NoState )
        aspect = aspect | skinState();

    return storedHint( aspect, status );
}

QskSkinHintStatus QskSkinnable::hintStatus( QskAspect::Aspect aspect ) const
{
    QskSkinHintStatus status;

    ( void ) effectiveHint( aspect, &status );
    return status;
}


QVariant QskSkinnable::animatedValue(
    QskAspect::Aspect aspect, QskSkinHintStatus* status ) const
{
    QVariant v;

    if ( aspect.state() == QskAspect::NoState )
    {
        /*
            The local animators were invented to be stateless
            and we never have an aspect with a state here.
            But that might change ...
         */

        v = m_data->animators.currentValue( aspect );
    }

    if ( !v.isValid() )
    {
        if ( QskSkinTransition::isRunning() &&
            !m_data->hintTable.hasHint( aspect ) )
        {
            /*
               Next we check for values from the skin. Those
               animators are usually from global skin/color changes
               and are state aware
             */

            if ( const auto control = owningControl() )
            {
                if ( aspect.state() == QskAspect::NoState )
                    aspect = aspect | skinState();

                const auto a = aspect;

                Q_FOREVER
                {
                    v = QskSkinTransition::animatedHint( control->window(), aspect );

                    if ( !v.isValid() )
                    {
                        if ( const auto topState = aspect.topState() )
                        {
                            aspect.clearState( aspect.topState() );
                            continue;
                        }

                        if ( aspect.placement() )
                        {
                            // clear the placement bits and restart
                            aspect = a;
                            aspect.setPlacement( QskAspect::NoPlacement );

                            continue;
                        }
                    }

                    break;
                }
            }
        }
    }

    if ( status && v.isValid() )
    {
        status->source = QskSkinHintStatus::Animator;
        status->aspect = aspect;
    }

    return v;
}

const QVariant& QskSkinnable::storedHint(
    QskAspect::Aspect aspect, QskSkinHintStatus* status ) const
{
    const auto skin = effectiveSkin();

    // clearing all state bits not being handled from the skin
    aspect.clearState( ~skin->stateMask() );

    QskAspect::Aspect resolvedAspect;

    const auto& localTable = m_data->hintTable;
    if ( localTable.hasHints() )
    {
        QskAspect::Aspect a = aspect;

        if ( !localTable.hasStates() )
        {
            // we don't need to clear the state bits stepwise
            a.clearStates();
        }

        if ( const QVariant* value = localTable.resolvedHint( a, &resolvedAspect ) )
        {
            if ( status )
            {
                status->source = QskSkinHintStatus::Skinnable;
                status->aspect = resolvedAspect;
            }
            return *value;
        }
    }

    // next we try the hints from the skin

    const auto& skinTable = skin->hintTable();
    if ( skinTable.hasHints() )
    {
        QskAspect::Aspect a = aspect;

        const QVariant* value = skinTable.resolvedHint( a, &resolvedAspect );
        if ( value )
        {
            if ( status )
            {
                status->source = QskSkinHintStatus::Skin;
                status->aspect = resolvedAspect;
            }

            return *value;
        }

        if ( aspect.subControl() != QskAspect::Control )
        {
            // trying to resolve something from the skin default settings

            aspect.setSubControl( QskAspect::Control );
            aspect.clearStates();

            value = skinTable.resolvedHint( aspect, &resolvedAspect );
            if ( value )
            {
                if ( status )
                {
                    status->source = QskSkinHintStatus::Skin;
                    status->aspect = resolvedAspect;
                }

                return *value;
            }
        }
    }

    if ( status )
    {
        status->source = QskSkinHintStatus::NoSource;
        status->aspect = QskAspect::Aspect();
    }

    static QVariant hintInvalid;
    return hintInvalid;
}

QskAspect::State QskSkinnable::skinState() const
{
    return m_data->skinState;
}

const char* QskSkinnable::skinStateAsPrintable() const
{
    return skinStateAsPrintable( skinState() );
}

const char* QskSkinnable::skinStateAsPrintable( QskAspect::State state ) const
{
    QString tmp;

    QDebug debug( &tmp );
    qskDebugState( debug, metaObject(), state );

    // we should find a better way
    static QByteArray bytes[ 10 ];
    static int counter = 0;

    counter = ( counter + 1 ) % 10;

    bytes[ counter ] = tmp.toUtf8();
    return bytes[ counter ].constData();
}

static inline QMarginsF qskEffectivePadding( const QskSkinnable* skinnable,
    QskAspect::Aspect aspect, const QSizeF& size, bool inner )
{
    using namespace QskAspect;
    using namespace Qt;

    const auto shape = skinnable->boxShapeHint( aspect | Shape ).toAbsolute( size );
    const auto borderMetrics = skinnable->boxBorderMetricsHint( aspect | Border );

    const qreal left = qMax( shape.radius( TopLeftCorner ).width(),
        shape.radius( BottomLeftCorner ).width() );

    const qreal top = qMax( shape.radius( TopLeftCorner ).height(),
        shape.radius( TopRightCorner ).height() );

    const qreal right = qMax( shape.radius( TopRightCorner ).width(),
        shape.radius( BottomRightCorner ).width() );

    const qreal bottom = qMax( shape.radius( Qt::BottomLeftCorner ).height(),
        shape.radius( Qt::BottomRightCorner ).height() );

    QMarginsF padding( left, top, right, bottom );

    // half of the border goes to the inner side
    const auto borderMargins = borderMetrics.toAbsolute( size ).widths() * 0.5;

    if ( inner )
    {
        padding -= borderMargins;
    }
    else
    {
        // not correct, but to get things started. TODO ...
        padding += borderMargins;
    }

    // sin 45° ceiled : 0.70710678;
    padding *= 1.0 - 0.70710678;

    const QMarginsF paddingHint = skinnable->marginsHint( aspect | Padding );

    return QMarginsF(
        qMax( padding.left(), paddingHint.left() ),
        qMax( padding.top(), paddingHint.top() ),
        qMax( padding.right(), paddingHint.right() ),
        qMax( padding.bottom(), paddingHint.bottom() )
        );
}

QMarginsF QskSkinnable::innerPadding(
    QskAspect::Aspect aspect, const QSizeF& outerBoxSize ) const
{
    return qskEffectivePadding( this, aspect, outerBoxSize, true );
}

QSizeF QskSkinnable::innerBoxSize(
    QskAspect::Aspect aspect, const QSizeF& outerBoxSize ) const
{
    const QMarginsF m = qskEffectivePadding( this, aspect, outerBoxSize, true );

    return QSizeF( outerBoxSize.width() - m.left() - m.right(),
        outerBoxSize.height() - m.top() - m.bottom() );
}

QRectF QskSkinnable::innerBox(
    QskAspect::Aspect aspect, const QRectF& outerBox ) const
{
    const QMarginsF m = qskEffectivePadding( this, aspect, outerBox.size(), true );
    return outerBox.marginsRemoved( m );
}

QSizeF QskSkinnable::outerBoxSize(
    QskAspect::Aspect aspect, const QSizeF& innerBoxSize ) const
{
    const QMarginsF m = qskEffectivePadding( this, aspect, innerBoxSize, false );

    return QSizeF( innerBoxSize.width() + m.left() + m.right(),
        innerBoxSize.height() + m.top() + m.bottom() );
}

QRectF QskSkinnable::outerBox(
    QskAspect::Aspect aspect, const QRectF& innerBox ) const
{
    const auto m = qskEffectivePadding( this, aspect, innerBox.size(), false );
    return innerBox.marginsAdded( m );
}

bool QskSkinnable::isTransitionAccepted( QskAspect::Aspect aspect ) const
{
    Q_UNUSED( aspect )

    /*
        Usually we only need smooth transitions, when state changes
        happen while the skinnable is visible. There are few exceptions
        like QskPopup::Closed, that is used to slide/fade in.
     */
    if ( auto control = owningControl() )
        return control->isInitiallyPainted();

    return false;
}

void QskSkinnable::startTransition( QskAspect::Aspect aspect,
    QskAnimationHint animationHint, QVariant from, QVariant to )
{
    if ( animationHint.duration <= 0 || ( from == to ) )
        return;

    auto control = this->owningControl();
    if ( control->window() == nullptr || !isTransitionAccepted( aspect ) )
        return;

    /*
        We might be invalid for one of the values, when an aspect
        has not been defined for all states ( f.e. metrics are expected
        to fallback to 0.0 ). In this case we create a default one.
     */

    if ( !from.isValid() )
    {
        from = qskTypedNullValue( to );
    }
    else if ( !to.isValid() )
    {
        to = qskTypedNullValue( from );
    }
    else if ( from.userType() != to.userType() )
    {
        return;
    }

    if ( aspect.flagPrimitive() == QskAspect::GraphicRole )
    {
        const auto skin = effectiveSkin();

        from.setValue( skin->graphicFilter( from.toInt() ) );
        to.setValue( skin->graphicFilter( to.toInt() ) );
    }

    aspect.clearStates();
    aspect.setAnimator( false );
    aspect.setPlacement( effectivePlacement() );

#if DEBUG_ANIMATOR
    qDebug() << aspect << animationHint.duration;
#endif

    auto animator = m_data->animators.animator( aspect );
    if ( animator && animator->isRunning() )
        from = animator->currentValue();

    m_data->animators.start( control, aspect, animationHint, from, to );
}

void QskSkinnable::setSkinStateFlag( QskAspect::State stateFlag, bool on )
{
    const auto newState = on
        ? ( m_data->skinState | stateFlag )
        : ( m_data->skinState & ~stateFlag );

    setSkinState( newState );
}

void QskSkinnable::setSkinState( QskAspect::State newState, bool animated )
{
    if ( m_data->skinState == newState )
        return;

    auto control = owningControl();

#if DEBUG_STATE
    qDebug() << control->className() << ":"
        << skinStateAsPrintable( m_data->skinState ) << "->"
        << skinStateAsPrintable( newState );
#endif

    const auto skin = effectiveSkin();

    if ( skin )
    {
        const auto mask = skin->stateMask();
        if ( ( newState & mask ) == ( m_data->skinState & mask ) )
        {
            // the modified bits are not handled by the skin

            m_data->skinState = newState;
            return;
        }
    }

    if ( control->window() && animated && isTransitionAccepted( QskAspect::Aspect() ) )
    {
        const auto placement = effectivePlacement();

        const auto subControls = control->subControls();
        for ( const auto subControl : subControls )
        {
            using namespace QskAspect;

            Aspect aspect = subControl | placement;

            const auto& skinTable = skin->hintTable();

            for ( int i = 0; i <= LastType; i++ )
            {
                const auto type = static_cast< Type >( i );

                const auto hint = effectiveAnimation( type, subControl, newState );

                if ( hint.duration > 0 )
                {
                    /*
                        Starting an animator for all primitives,
                        that differ between the states
                     */
                    for ( uint primitive = 0; primitive <= LastPrimitive; primitive++ )
                    {
                        aspect.setPrimitive( type, primitive );

                        Aspect a1 = aspect | m_data->skinState;
                        Aspect a2 = aspect | newState;

                        bool doTransition = true;

                        if ( !m_data->hintTable.hasStates() )
                        {
                            /*
                                The hints are found by stripping the state bits one by
                                one until a lookup into the hint table is successful.
                                So for deciding whether two aspects lead to the same hint
                                we can stop as soon as the aspects have the same state bits.
                                This way we can reduce the number of lookups significantly
                                for skinnables with many state bits.

                             */
                            doTransition = !qskCompareResolvedStates( a1, a2, skinTable );
                        }

                        if ( doTransition )
                        {
                            startTransition( aspect, hint,
                                storedHint( a1 ), storedHint( a2 ) );
                        }
                    }
                }
            }
        }
    }

    m_data->skinState = newState;

    if ( control->flags() & QQuickItem::ItemHasContents )
        control->update();
}

QskSkin* QskSkinnable::effectiveSkin() const
{
    QskSkin* skin = nullptr;

    if ( m_data->skinlet )
        skin = m_data->skinlet->skin();

    return skin ? skin : qskSetup->skin();
}

void QskSkinnable::updateNode( QSGNode* parentNode )
{
    effectiveSkinlet()->updateNode( this, parentNode );
}

QskAspect::Subcontrol QskSkinnable::effectiveSubcontrol(
    QskAspect::Subcontrol subControl ) const
{
    // derived classes might want to redirect a sub-control
    return subControl;
}

QskControl* QskSkinnable::controlCast()
{
    return qskIsControl( this )
        ? static_cast< QskControl* >( this ) : nullptr;
}

const QskControl* QskSkinnable::controlCast() const
{
    return qskIsControl( this )
        ? static_cast< const QskControl* >( this ) : nullptr;
}

void QskSkinnable::debug( QDebug debug, QskAspect::Aspect aspect ) const
{
    qskDebugAspect( debug, metaObject(), aspect );
}

void QskSkinnable::debug( QDebug debug, QskAspect::State state ) const
{
    qskDebugState( debug, metaObject(), state );
}

void QskSkinnable::debug( QskAspect::Aspect aspect ) const
{
    qskDebugAspect( qDebug(), metaObject(), aspect );
}

void QskSkinnable::debug( QskAspect::State state ) const
{
    qskDebugState( qDebug(), metaObject(), state );
}

#ifndef QT_NO_DEBUG_STREAM

#include <qdebug.h>

QDebug operator<<( QDebug debug, const QskSkinHintStatus& status )
{
    QDebugStateSaver saver( debug );
    debug.nospace();

    switch( status.source )
    {
        case QskSkinHintStatus::Skinnable:
            debug << "Skinnable";
            break;
        case QskSkinHintStatus::Skin:
            debug << "Skin";
            break;
        case QskSkinHintStatus::Animator:
            debug << "Animator";
            break;
        default:
            debug << "None";
            break;
    }

    debug << ": " << status.aspect;

    return debug;
}

#endif
