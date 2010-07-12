/*
 * Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, version 2.1 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not,
 * see "http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html/".
 *
 * Description:
 *
 */

#include "EditorWidget.h"
#include "Utilities.h"
#include "ChromeEffect.h"

// FIXME ;;; Must address the following issues:
//
// * On Symbian, when focus is outside editor and user clicks inside editor, the editor
//   receives FocusIn event but does not make the blinking cursor visible until
//   the user starts to type or presses arrow keys.
//
// * Edit selection is not visible.
//


namespace GVA {

  // Methods for class EditorItem

  //GTextLineItem extends QGraphicsTextItem as a single-line editor.
  //Signals horizontal cursor movement, which can be used to implement horizontal scrolling.

  GTextLineItem::GTextLineItem(QGraphicsItem * parent)
  : QGraphicsTextItem(parent)
  , m_maxTextLength(0)
  , m_hints(Qt::ImhNoPredictiveText) // disable the predictive text
  {
    // Disable wrapping, force text to be stored and displayed
    // as a single line.
    QTextOption textOption = document()->defaultTextOption();
    textOption.setWrapMode(QTextOption::NoWrap);
    document()->setDefaultTextOption(textOption);
    // Enable cursor keys.
    setTextInteractionFlags(Qt::TextEditorInteraction);
    // This is needed to initialize m_textLine.
    setText("");
    setAcceptDrops(false);
    m_defaultStartDragDistance = QApplication::startDragDistance();
  }

  GTextLineItem::~GTextLineItem()
  {
    if (m_maxTextLength > 0)
      disconnect(document(), SIGNAL(contentsChange(int, int, int)), this, SLOT(contentsChange(int, int, int)));
    QApplication::setStartDragDistance(m_defaultStartDragDistance);
  }

  void GTextLineItem::setText(const QString & text)
  {
    QString newText = text;
    if(m_maxTextLength > 0 && text.length() > m_maxTextLength ) {
        newText = newText.left(m_maxTextLength);
    }
    setPlainText(newText);
    m_textLine = document()->begin().layout()->lineForTextPosition(0);
  }

  void GTextLineItem::setMaxTextLength(int length)
  {
    if (m_maxTextLength <= 0 && length > 0) {
        QString text = toPlainText();
        if( text.length() > length ) {
          setPlainText(text.left(length));
        }
        connect(document(), SIGNAL(contentsChange(int, int, int)), this, SLOT(contentsChange(int, int, int)));
    } else if (m_maxTextLength > 0 && length <= 0) {
        disconnect(document(), SIGNAL(contentsChange(int, int, int)), this, SLOT(contentsChange(int, int, int)));
    }
    m_maxTextLength = length;
  }

  void GTextLineItem::contentsChange(int position, int charsRemoved, int charsAdded)
  {
    Q_UNUSED(position);
    Q_UNUSED(charsRemoved);
    Q_UNUSED(charsAdded);
    if( m_maxTextLength > 0 && toPlainText().length() > m_maxTextLength ) {
      QTextCursor cursor = textCursor();
      cursor.deletePreviousChar();
      setTextCursor(cursor);
    }
  }

  // Get the pixel offset of the cursor. Needed to implement scrolling.

  qreal GTextLineItem::cursorX()
  {
    return m_textLine.cursorToX(textCursor().position());
  }

  qreal GTextLineItem::anchorX()
  {
    return m_textLine.cursorToX(textCursor().anchor());
  }

  QRectF GTextLineItem::selectionRectF()
  {
    qreal x1 = cursorX();
    qreal x2 = anchorX();
    if (x1 == x2)
      return QRectF();
    return QRectF(x1, 0, x2-x1 + 16, boundingRect().height());
  }

  void GTextLineItem::selectAll()
  {
    QTextCursor tc = textCursor();
    tc.select(QTextCursor::Document);
    setTextCursor(tc);
    emit cursorXChanged(cursorX());
  }

  void GTextLineItem::unselect()
  {
    QTextCursor tc = textCursor();
    tc.setPosition(tc.position());
    setTextCursor(tc);
  }

  void GTextLineItem::setCursorPosition(int pos)
  {
    QTextCursor tc = textCursor();
    tc.setPosition(pos);
    setTextCursor(tc);
  }

  qreal GTextLineItem::textWidth()
  {
    return m_textLine.naturalTextWidth();
  }

  void GTextLineItem::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget)
  {
    // Paint without ugly selection ants (the dashed line that surrounds
    // the selected text). TODO: It is clearly a bug in QGraphicsTextItem
    // that you cannot explicitly set the style of the selection indicator.  File this.
    QStyleOptionGraphicsItem newOption = *option;
    newOption.state &= (!QStyle::State_Selected | !QStyle::State_HasFocus);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    QGraphicsTextItem::paint(painter, &newOption, widget);
    painter->restore();
    if(!isEnabled()) {
        ChromeEffect::paintDisabledRect(painter, option->exposedRect);
    }
  }

  void GTextLineItem::mouseMoveEvent(QGraphicsSceneMouseEvent * event)
  {
    QGraphicsTextItem::mouseMoveEvent(event);
    emit cursorXChanged(cursorX());
  }

  void GTextLineItem::mousePressEvent(QGraphicsSceneMouseEvent * event)
  {
    QGraphicsTextItem::mousePressEvent(event);
  }

  void GTextLineItem::mouseReleaseEvent(QGraphicsSceneMouseEvent * event)
  {
    QGraphicsTextItem::mouseReleaseEvent(event);
    QPointF pos = event->pos();
    emit tapped(pos);
    // open vkb by single tap
    QWidget * widget = event->widget();
    QEvent vkbEvent(QEvent::RequestSoftwareInputPanel);
    QApplication::sendEvent(widget, &vkbEvent);
  }

  void GTextLineItem::keyPressEvent(QKeyEvent * event)
  {
    // Signal horizontal cursor movement so that an editor widget can
    // implement horizontal scrolling.
    qreal oldX = cursorX();
    QGraphicsTextItem::keyPressEvent(event);
    qreal newX = cursorX();
    if (newX != oldX) {
      emit cursorXChanged(newX);
    }
  }

  void GTextLineItem::keyReleaseEvent(QKeyEvent * event)
  {
    QGraphicsTextItem::keyReleaseEvent(event);
    emit textMayChanged();
  }

  void GTextLineItem::focusInEvent(QFocusEvent * event)
  {
    Q_UNUSED(event);
    // disable the drag & drop to fix the auto-delete-all issue
    QApplication::setStartDragDistance(1000);
    QGraphicsTextItem::focusInEvent(event);

    QWidget* fw = QApplication::focusWidget();
    Qt::InputMethodHints hints = fw->inputMethodHints();
    if (hints != m_hints)
      fw->setInputMethodHints(m_hints);

    if (event->reason() != Qt::PopupFocusReason) // to fix the special char issue on VKB
      emit focusChanged(true);
  }

  void GTextLineItem::focusOutEvent(QFocusEvent * event)
  {
    Q_UNUSED(event);
    // restore the drag & drop for other components
    QApplication::setStartDragDistance(m_defaultStartDragDistance);
    QGraphicsTextItem::focusOutEvent(event);
    if (event->reason() != Qt::PopupFocusReason && event->reason() != Qt::ActiveWindowFocusReason) // to fix the special char issue on VKB
      emit focusChanged(false);
  }

  void GTextLineItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
  {
    // ignore context menu event
    event->ignore();
  }

  void GTextLineItem::setInputMethodHints(Qt::InputMethodHints hints)
  { 
    m_hints = hints;
    m_hints |= Qt::ImhNoPredictiveText;  // disable the predictive text
  }

  // Methods for class GLineEditor
  // GLineEditor is a QGraphicsWidget that wraps a GTextLineItem to implement scrolling, 
  // draw a background and set padding 

  GLineEditor::GLineEditor(ChromeSnippet * snippet, ChromeWidget * chrome, QGraphicsItem * parent)
  : QGraphicsWidget(parent)
  , m_chrome(chrome)
  , m_viewPortWidth(0.0)
  , m_viewPortHeight(0.0)
  , m_padding(0.0)
  , m_rightTextMargin(0.0)
  {
    Q_UNUSED(snippet);

    // The viewport clips the editor when text overflows
    // viewport size will be set in resize()
    m_viewPort = new QGraphicsWidget(this);
    m_viewPort->setFlags(QGraphicsItem::ItemClipsChildrenToShape);

    // The actual text editor item
    m_editor = new GTextLineItem(m_viewPort);
    m_editor->setDefaultTextColor(m_textColor);
    m_editor->installEventFilter(this);

    // Monitor editor cursor position changes for horizontal scrolling.
    safe_connect(m_editor, SIGNAL(cursorXChanged(qreal)),
                 this, SLOT(makeVisible(qreal)));

    safe_connect(m_editor, SIGNAL(textMayChanged()),
                 this, SIGNAL(textMayChanged()));

    safe_connect(m_editor, SIGNAL(focusChanged(bool)),
                 this, SIGNAL(focusChanged(bool)));
    
    safe_connect(m_editor, SIGNAL(tapped(QPointF&)),
                 this, SIGNAL(tapped(QPointF&)));

    setAcceptDrops(false);
  }

  GLineEditor::~GLineEditor()
  {
  }

  // TODO: Be a good QGraphicsWidget: update this to use palette color?

  void GLineEditor::setTextColor(QColor & color)
  {
    m_textColor = color;
    m_editor->setDefaultTextColor(m_textColor);
  }

  void GLineEditor::setPadding(qreal padding)
  {
    m_padding = padding;
    //Trigger a resize to adjust component sizes to new padding
    resize(size());
  }

  void GLineEditor::setRightTextMargin(qreal margin)
  {
    m_rightTextMargin = margin;
    resize(size());
  }

  QString GLineEditor::text() const
  {
    return m_editor->toPlainText();
  }

  void GLineEditor::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget)
  {
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    // First, fill rectangle with background color.
    painter->fillRect(boundingRect(), m_backgroundColor);
    painter->restore();
    if(!isEnabled()) {
        ChromeEffect::paintDisabledRect(painter, option->exposedRect);
    }
    // Make sure any required horizontal scrolling happens
    // before rendering editor widget, which will be drawn on top
    // of the background rectangle.
    makeVisible(m_editor->cursorX());
  }

  void GLineEditor::resizeEvent(QGraphicsSceneResizeEvent * event)
  {
    QSizeF size = event->newSize();
    m_viewPortWidth  = size.width() - m_rightTextMargin  - m_padding * 2;
    m_viewPortHeight = size.height() - m_padding * 2;
    m_viewPort->setGeometry(
                            m_padding,
                            (size.height() - m_editor->boundingRect().height()) / 2,
                            m_viewPortWidth,
                            m_viewPortHeight);
    m_editor->setTextWidth(m_viewPortWidth);
    // move back the m_editor'x to 0
    qreal editorShift = -1 * m_editor->pos().x();
    m_editor->moveBy(editorShift, 0);
    updateEditor();
  }

  void GLineEditor::setText(const QString & text)
  {
    m_editor->setText(text);
    m_editor->setPos(0, m_editor->pos().y());
    updateEditor();
  }

  void GLineEditor::updateEditor()
  {
    makeVisible(m_editor->cursorX());
  }

  // We divide the viewport into 3 distinct regions:
  //
  //
  //        [ left | middle | right ]
  //
  // [ editor, shifted left by editorShift pixels ]
  //
  // When a cursor is in the middle section of the viewport we
  // leave the editor shift unchanged, to preserve stability.
  //
  // When a cursor is in the right section or beyond we shift
  // the editor left until the cursor appears at the border
  // between the middle and right sections.
  //
  // When a cursor is in the left section or beyond we shift
  // the editor right until the cursor appears at the border
  // between the left and middle sections.
  //
  // We never shift the editor right of the viewport.

  void GLineEditor::makeVisible(qreal cursorX)
  {
    qreal leftScrollBorder  = 0;
    qreal rightScrollBorder = m_viewPortWidth - 20;
    qreal editorShift = -1 * m_editor->pos().x();
    qreal localX = cursorX - editorShift;

    if (m_editor->textWidth() < rightScrollBorder) {
      if (editorShift !=0)
        m_editor->moveBy(editorShift, 0);
      return;
    }
    
    if (m_editor->textWidth() - editorShift < rightScrollBorder) {           
      if (editorShift != 0)
        m_editor->moveBy(rightScrollBorder - m_editor->textWidth() + editorShift, 0);
      return;
    }

    if (localX < leftScrollBorder) {
      // Before left section, scroll right.
      // In left section, scroll right.
      qreal shift = qMin(leftScrollBorder - localX, editorShift);
      m_editor->moveBy(shift, 0);
      return;
    }
    if (localX >= rightScrollBorder) {
      // In right section, scroll left.
      // After right section, scroll left.
      qreal shift = localX - rightScrollBorder;
      m_editor->moveBy(-shift, 0);
      return;
    }
    // In middle section, no scroll needed.
    return;
  }

  bool GLineEditor::tappedOnText(qreal x) const
  {
    qreal editorShift = m_editor->pos().x();
    qreal editorWidth = m_editor->textWidth();
    if (editorShift == 0 && editorWidth < x)
      return false;
    return true;
  }

  bool GLineEditor::eventFilter(QObject * object, QEvent * event)
  {
    // Filter editor key events.
    if (object != m_editor)
      return false;

    if (event->type() != QEvent::KeyPress)
      return false;

    QKeyEvent * keyEvent = static_cast<QKeyEvent*>(event);
    switch (keyEvent->key()) {
    case Qt::Key_Select:
    case Qt::Key_Return:
    case Qt::Key_Enter:
      // Signal that a carriage return-like key-press happened.
      emit activated();
      return true;
    case Qt::Key_Down:
    case Qt::Key_Up:
      // Swallow arrow up/down keys, editor has just one line.
      return true;
    default:
      return false;
    }
  }

  //GTextEditor paints a styled frame around a GLineEditor

  GTextEditor::GTextEditor(ChromeSnippet * snippet, ChromeWidget * chrome, QGraphicsItem * parent)
  : GLineEditor(snippet, chrome, parent)
  {
  }

  GTextEditor::~GTextEditor()
  {
  }

  void GTextEditor::paintBorder(QPainter * painter)
  {
    if (m_padding > 0 ) {
      QPainterPath border;
      //qDebug() << "GTextEditor::paintBorder: " << boundingRect().width() << " " << size().width();
      border.addRect(boundingRect());
      border.addRoundedRect(
                            m_padding,
                            m_padding,
                            size().width()-m_padding*2,
                            size().height()-m_padding*2,
                            4,
                            4);
      painter->fillPath(border, m_borderColor);
    }
  }

  void GTextEditor::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget)
  {
    Q_UNUSED(option);
    Q_UNUSED(widget);

    //updateEditor();
    GLineEditor::paint(painter, option, widget);
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    paintBorder(painter);
    painter->restore();
  }

  //GProgressEditor adds a progress bar to GTextEditor. Joining these into a single QGraphicsWidget makes it
  //possible to draw a text editor with a progress bar with minimal updates

  GProgressEditor::GProgressEditor(ChromeSnippet * snippet, ChromeWidget * chrome, QGraphicsItem * parent)
  : GTextEditor(snippet, chrome, parent)
  , m_percent(0)
  {
  }

  GProgressEditor::~GProgressEditor()
  {
  }

  void GProgressEditor::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget)
  {
    Q_UNUSED(option);
    Q_UNUSED(widget);

    updateEditor();

    QRectF progressRect = boundingRect();
    progressRect.setWidth(progressRect.width() * m_percent / 100.0);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->fillRect(boundingRect(), m_backgroundColor);
    painter->fillRect(progressRect, m_progressColor);
    paintBorder(painter);
    painter->restore();
    if(!isEnabled()) {
        ChromeEffect::paintDisabledRect(painter, option->exposedRect);
    }
  }

  void GProgressEditor::setProgress(int percent)
  {
    // Don't bother with small increments, but ...
    //
    // - ALWAYS show the final increment to 99 or 100, because
    //   this tells the user a network request has completed.
    //
    // - ALWAYS show the initial increment from 0, because this
    //   tells the user a new network request has started.
    //
    // - ALWAYS show decrements, because this tells the user
    //   a new network request has started.
    if (percent < 99) {
      if (m_percent > 0) {
        if (percent > m_percent) {
          if (percent - m_percent < 10) {
            //qDebug() << "UrlSearchSnippet::setProgress" << percent << "IGNORE";
            return;
          }
        }
      }
    }

    if (m_percent == percent) {
      //qDebug() << "UrlSearchSnippet::setProgress" << percent << "IGNORE";
      return;
    }

    //qDebug() << "UrlSearchSnippet::setProgress" << percent << "UPDATE";
    m_percent = percent;
    update();
  }

  //A chrome item that displays a GTextEditor. This can be embedded in HTML chrome instead of an input field.

  TextEditItem::TextEditItem(ChromeSnippet * snippet, ChromeWidget * chrome, QGraphicsItem * parent)
    : NativeChromeItem(snippet, parent)
	, m_justFocusIn(false)
  {
    m_textEditor = new GTextEditor(snippet, chrome, this);
  
    //Style via CSS
    QWebElement we = m_snippet->element();

    QColor textColor;
    NativeChromeItem::CSSToQColor(we.styleProperty("color", QWebElement::ComputedStyle),
                                  textColor);
    m_textEditor->setTextColor(textColor);

    //Background of text box 
    QColor backgroundColor;
    NativeChromeItem::CSSToQColor(we.styleProperty("background-color", QWebElement::ComputedStyle),
                                  backgroundColor); 
    m_textEditor->setBackgroundColor(backgroundColor);

    QColor borderColor;
    NativeChromeItem::CSSToQColor(we.styleProperty("border-top-color", QWebElement::ComputedStyle),
                                  borderColor);
    m_textEditor->setBorderColor(borderColor);

    //Padding sets the "border" width
    QString cssPadding = we.styleProperty("padding-top", QWebElement::ComputedStyle);
    m_textEditor->setPadding(cssPadding.remove("px").toInt());

	safe_connect(m_textEditor, SIGNAL(focusChanged(bool)),this, SLOT(focusChanged(bool)));
    safe_connect(m_textEditor, SIGNAL(tapped(QPointF&)),this, SLOT(tapped(QPointF&)));
}
  
  void TextEditItem::tapped(QPointF& pos)
{
    bool hitText = m_textEditor->tappedOnText(pos.x());
    if (!m_justFocusIn && !hitText)
        m_textEditor->unselect();

    if (m_justFocusIn) {
        m_justFocusIn = false;
        if (hitText && !m_textEditor->hasSelection())
            m_textEditor->selectAll();
    }
}

void TextEditItem::focusChanged(bool focusIn)
{
    if (focusIn)
        m_justFocusIn = true;
    else {
        m_justFocusIn = false;
        m_textEditor->unselect();
        m_textEditor->shiftToLeftEnd();
    }
}

  TextEditItem::~TextEditItem()
  {
    delete m_textEditor;
  }

  void TextEditItem::resizeEvent(QGraphicsSceneResizeEvent * ev)
  {
    m_textEditor->resize(ev->newSize());
  }
  
  void TextEditItem::setTextOptions(int flag)
  {
    m_textEditor->setInputMethodHints((Qt::InputMethodHints)flag);
  }

} // namespace GVA
