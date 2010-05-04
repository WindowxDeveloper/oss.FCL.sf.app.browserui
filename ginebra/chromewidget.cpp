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

#include <QtGui>
#include <QNetworkReply>
#include <QGraphicsScene>
#include <QImage>
#include <QTimeLine>
#include <QPainterPath>
#include "qwebpage.h"
#include "qwebframe.h"
#include "qwebview.h"
#include "qwebelement.h"
#include "chromewidget.h"
#include "chromerenderer.h"
#include "chromesnippet.h"
#include "chromewidgetjsobject.h"
#include "chromeview.h"
#include "attentionanimator.h"
#include "visibilityanimator.h"
//NB: remove these
#include "animations/fadeanimator.h"
#include "animations/bounceanimator.h"
#include "animations/flyoutanimator.h"

#include "utilities.h"
#include <assert.h>

//Temporary include
#include <QDebug>

#ifdef G_TIMING
#include "gtimer.h"
#endif

class UpdateBufferEvent : public QEvent {
  public:
    UpdateBufferEvent()
      : QEvent(customType()) {
    }
    static QEvent::Type customType() {
        static int type = QEvent::registerEventType();
        return (QEvent::Type) type;
    }
};

ChromeWidget::ChromeWidget(ChromeView *parentChromeView, QGraphicsItem *parent, const QString &jsName)
  :QObject(),
   m_chromePage(0),
   m_parentItem(parent),
   m_parentChromeView(parentChromeView),
   m_state(maximized),
   m_buffer(0),
   m_painter(0),
   m_dirtyTimer(0),
   m_jsObject(new ChromeWidgetJSObject(this, this, jsName))
{
  // Connect signals generated by this object to signals on the javascript object.
  safe_connect(this, SIGNAL(loadStarted()), m_jsObject, SIGNAL(loadStarted()));
  safe_connect(this, SIGNAL(loadComplete()), m_jsObject, SIGNAL(loadComplete()));
  safe_connect(this, SIGNAL(dragStarted()), m_jsObject, SIGNAL(dragStarted()));
  safe_connect(this, SIGNAL(dragFinished()), m_jsObject, SIGNAL(dragFinished()));
  safe_connect(this, SIGNAL(viewPortResize(QRect)), m_jsObject, SIGNAL(viewPortResize(QRect)));

  //Allocate an instance of webkit to render the chrome
  ChromeRenderer *pageView = new ChromeRenderer(parentChromeView->parentWidget());
  safe_connect(pageView, SIGNAL(symbianCarriageReturn()), m_jsObject, SIGNAL(symbianCarriageReturn()));
  //QWebView *pageView = new QWebView(parentChromeView->parentWidget());

  pageView->show();
  m_chromePage = pageView->page();

  //Render to a transparent background (see WebKit bug 29414)
  QPalette palette = m_chromePage->palette();
  palette.setColor(QPalette::Base, Qt::transparent);
  m_chromePage->setPalette(palette);

  // No scrolling of the chrome
  m_chromePage->mainFrame()->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
  m_chromePage->mainFrame()->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);

  // Display the page view offscreen to ensure that it doesn't grab focus from chrome widget
  pageView->setGeometry(-1600, 0, 5, 5);

  // Connect QtWebPage signals
  safe_connect(chromePage(), SIGNAL(frameCreated(QWebFrame*)), this, SLOT(frameCreated(QWebFrame*)));
  safe_connect(chromePage(), SIGNAL(loadStarted()), this, SLOT(onLoadStarted()));
  safe_connect(chromePage(), SIGNAL(loadFinished(bool)), this, SLOT(loadFinished(bool)));
  safe_connect(chromePage(), SIGNAL(repaintRequested(QRect)), this, SLOT(repaintRequested(QRect)));

  //External links in chrome are delegated to a content view so we
  //propagate the QWebPage::linkClicked signal. The idea is that
  //chrome does not normally load external content into the chrome
  //itself (though chrome can still load content via XHR). Typically,
  //the active  web view handles this signal. This allows chrome to contain
  //links to external content that get loaded into the web view. An
  //example would be a news feed that renders feed item titles in a chrome
  //pop-up, but which loads the feed content into the main web view.

  chromePage()->setLinkDelegationPolicy(QWebPage::DelegateExternalLinks);
  QObject::connect(chromePage(), SIGNAL(linkClicked(const QUrl&)), this, SIGNAL(delegateLink(const QUrl&)));

  installEventFilter(this);
}

ChromeWidget::~ChromeWidget(){
  delete m_painter;
  delete m_jsObject; 
  delete m_buffer;
}


QObject *ChromeWidget::jsObject() { 
  return static_cast<QObject *>(m_jsObject); 
}

#ifdef Q_OS_SYMBIAN
QPixmap * ChromeWidget::buffer()
#else
QImage * ChromeWidget::buffer()
#endif
{
  return m_buffer;
}

QPainter * ChromeWidget::painter(){
  return m_painter;
}

void ChromeWidget::resizeBuffer(){
  //qDebug() << "ChromeWidget::resizeBuffer: " << chromePage()->mainFrame()->contentsSize();
  if(m_painter) {
      m_painter->end();
      delete m_painter;
      m_painter = 0;
  }
  if(m_buffer) 
      delete m_buffer;
#ifdef Q_OS_SYMBIAN
  m_buffer = new QPixmap(chromePage()->mainFrame()->contentsSize());
#else
  m_buffer = new QImage(chromePage()->mainFrame()->contentsSize(), QImage::Format_ARGB32_Premultiplied);
#endif
  m_buffer->fill(Qt::transparent);  
  m_painter = new QPainter(m_buffer);
}

void ChromeWidget::setChromeUrl(QString url)
{
  //qDebug() << "ChromeWidget::setChromeUrl: " << url;
  if(chromePage() && chromePage()->mainFrame()){
#ifdef G_TIMING
    GTimer * t = new GTimer();
    t->start("ChromeWidget::setChromeUrl()");
#endif
    chromePage()->mainFrame()->load(QUrl(url));
#ifdef G_TIMING
   t->stop();
   t->save();
   delete t;
#endif
  }
}

void ChromeWidget::setGeometry(const QRect &rect)
{
  m_chromePage->setViewportSize(QSize(rect.size().width(), 1000));
  resizeBuffer();
  updateChildGeometries();
}


void ChromeWidget::toggleVisibility(const QString & elementId)
{
  ChromeSnippet * snippet = getSnippet(elementId);
  if(snippet)
    snippet->toggleVisibility();
}

void ChromeWidget::setLocation(const QString& id, int x, int y)
{
  ChromeSnippet * snippet = getSnippet(id);
  if(snippet)
    snippet->setPos(x,y);
}

void ChromeWidget::setAnchor(const QString& id, const QString& anchor){
  ChromeSnippet * snippet = getSnippet(id);
  if(snippet)
    snippet->setAnchor(anchor);
}

void ChromeWidget::show(const QString & elementId, int x, int y)
{
  ChromeSnippet * snippet = getSnippet(elementId);

  if(snippet){
    snippet->setPos(x,y);
    snippet->show(true);
  }
}

void ChromeWidget::show(const QString & elementId)
{
  ChromeSnippet * snippet = getSnippet(elementId);

  if(snippet){
    //snippet->show(true);
     snippet->show(false);
  }
}


void ChromeWidget::hide(const QString & elementId)
{
  ChromeSnippet * snippet = getSnippet(elementId);
  if(snippet)
    snippet->hide(true);
}

void ChromeWidget::toggleAttention(const QString & elementId){
  ChromeSnippet * snippet = getSnippet(elementId);
  if(snippet) {
    //qDebug() << "ChromeWidget::toggleAttention " << elementId; 
    snippet->toggleAttention();
  }
}

void ChromeWidget::setVisibilityAnimator(const QString& elementId, const QString & animatorName){
 ChromeSnippet * snippet = getSnippet(elementId);
  if(snippet) {
    VisibilityAnimator * animator = VisibilityAnimator::create(animatorName, snippet);
    snippet->setVisibilityAnimator(animator); // NB: Move this to visibility animator implementation
  }
}

void ChromeWidget::setAttentionAnimator(const QString& elementId, const QString & animatorName){
ChromeSnippet * snippet = getSnippet(elementId);
  if(snippet) {
     AttentionAnimator * animator = AttentionAnimator::create(animatorName, snippet);
    snippet->setAttentionAnimator(animator); // NB: Move this to visibility animator implementation
  }
}

//NB: Factor out snippet cleanup and use in destructor too

void ChromeWidget::onLoadStarted()  // slot
{
  qDebug() << "ChromeWidget::onLoadStarted";
#ifdef G_TIMING
  GTimer * t = new GTimer();
  t->start("ChromeWidget::loadStarted");
#endif
  //First zero out all of the non-root snippets. These
  //will be deleted when the root snippets are deleted.
  QMapIterator<QString, ChromeSnippet*> i(m_snippetMap);
  while(i.hasNext()){
    i.next();
    if(i.value()->parentItem() != m_parentItem){
      m_snippetMap[i.key()] = 0;
    }
  }
  //Now delete the root snippets.
  foreach(ChromeSnippet *snippet, m_snippetMap){
    if(snippet){
      //Remove about-to-be-deleted snippet from parent scene
      m_parentChromeView->getScene()->removeItem(snippet);
      delete snippet;
    }
  }
  m_snippetMap.clear();
//  m_topSnippet = 0;
//  m_bottomSnippet = 0;
  //m_popSnippet = 0;
  //Does anybody care about this signal?
  emit loadStarted();
#ifdef G_TIMING
  t->stop();
  t->save();
  delete t;
#endif
}


QString ChromeWidget::getDisplayMode() 
{
	return m_parentChromeView->getDisplayMode(); 
	
}
	
	
void ChromeWidget::frameCreated(QWebFrame* frame){
  Q_UNUSED(frame)
  //qDebug() << "===>ChromeWidget::frameCreated";
}

void ChromeWidget::loadFinished(bool ok)  // slot
{
#ifdef G_TIMING
  GTimer * t = new GTimer();
  t->start("ChromeWidget::loadFinished");
#endif
  qDebug() << "ChromeWidget::loadFinished";
  if(!ok)
  {
    qDebug() << "ChromeWidget::loadFinished: error";
    return;
  }
  getInitialChrome();
  resizeBuffer();
  updateChildGeometries();
  emit loadComplete();
#ifdef G_TIMING
  t->stop();
  t->save();
  delete t;
#endif
}

void ChromeWidget::getInitialChrome(){
  
  QWebElement doc = chromePage()->mainFrame()->documentElement();
#if QT_VERSION < 0x040600
  QList <QWebElement> initialSnippets = doc.findAll(".GinebraSnippet");
#else
  QList <QWebElement> initialSnippets = doc.findAll(".GinebraSnippet").toList();
#endif
  foreach(QWebElement element, initialSnippets) {
    ChromeSnippet* s = getSnippet(element.attribute("id"));
    if((element.attribute("data-GinebraVisible","false"))=="true"){
      s->show(false);
    }
    else {
      s->hide();
    }
  }
}

ChromeSnippet *ChromeWidget::getSnippet(const QString &docElementId, QGraphicsItem *parent) {
 
  ChromeSnippet *result = m_snippetMap.value(docElementId);
  if(!result){
    QWebElement doc = chromePage()->mainFrame()->documentElement();
    QWebElement element = doc.findFirst("#" + docElementId);
    QRect rect = getDocElementRect(docElementId);
    if(!rect.isNull()){
      QGraphicsItem * p = (parent)?parent:m_parentItem;
      
      // Create the snippet, pass the ChromeWidget's javascript object in so that it can
      // be used as the parent of the snippet's javascript object.
      result = new ChromeSnippet(p, this, jsObject(), docElementId);
      
      // Make sure snippets are shown above the content view.
      result->setZValue(3);
      
      //result->setAnchor("AnchorCenter");
      //qDebug() << "Creating snippet: " << docElementId << ":" << (int) result;  
      // Set up connections to freeze the main content page while snippets are being dragged
      // to improve performance on complex pages.
      safe_connect(result->getJSObject(), SIGNAL(dragStarted()), this, SIGNAL(dragStarted()));
      safe_connect(result->getJSObject(), SIGNAL(dragFinished()), this, SIGNAL(dragFinished()));
      //Note that the following can be inefficient if several snippets are
      //made visible/invisible at once, which will result is successive
      //updates to the viewport. Optimize this to coalesce updates.
      safe_connect(result->getJSObject(), SIGNAL(onHide()), this, SLOT(updateViewPort()));
      safe_connect(result->getJSObject(), SIGNAL(onShow()), this, SLOT(updateViewPort()));
      //qDebug() << "Snippet child count: " << p->childItems().size() << " parent=" << ((QGraphicsWidget *)p)->objectName();
      //qDebug() << "ChromeWidget::getSnippet: " << docElementId << " " << rect;

      result->setOwnerArea(rect);
      //Snippet size is determined by owner area.
      result->resize(rect.size());
      //Set auto-layout attributes
      result->setAnchor(element.attribute("data-GinebraAnchor", "AnchorNone"));
      result->setHidesContent( element.attribute("data-GinebraHidesContent", "false") == "true" );
      result->setAnchorOffset( element.attribute("data-GinebraAnchorOffset", "0").toInt() ); //toInt() returns 0 for malformed string
      m_snippetMap[docElementId] = result;
      //NB: not very efficient
      QList <QVariant> chromeButtons = getChildIdsByClassName(docElementId, "GinebraButtonSnippet").toList();
      //qDebug() << "Chrome row size: " << chromeButtons.size();
      for(int i = 0; i < chromeButtons.size();i++) {
        qDebug() << "Chrome row button: " << chromeButtons[i].toString();
        getSnippet(chromeButtons[i].toString(),result);
      }
  
    }
    else{
      //qDebug() << "ChromeWidget::getSnippet: snippet not found, id=" << docElementId;
      return 0;
    }
  }else{
    //qDebug() << "Found existing snippet: " << docElementId;
  }

  return result;
}

/* Do a re-layout of the chrome. This gets snippet geometries, sets positions
 * and calculates the viewport size. This gets called when:
 * - New chrome is loaded
 * - The chrome is resized
 * This doesn't get called when chrome snippet visibility changes
 * or snippets get moved so that animations don't invoke multiple
 * relayouts. This means that visiblity changes need to explicitly
 * invoke a viewport size calculation if they want to resize the
 * viewport.
 */

void ChromeWidget::updateChildGeometries()
{
  QRect viewRect(QPoint(0,0), m_parentChromeView->geometry().size());
  //qDebug() << "ChromeWidget::updateChildGeometries: viewRect=" << viewRect;
  //m_chromePage->setViewportSize(viewRect.size());

  updateOwnerAreas();
  
  //NB: It would be more efficient to calculate the viewport as snippet geometry is set
  //though this ought to be done without duplicating code between here and updateViewport()
  
  foreach(ChromeSnippet *snippet, m_snippetMap) {
    qreal sHeight = snippet->ownerArea().height();
    if(snippet->anchor()=="AnchorTop"){
      snippet->setPos(0, snippet->anchorOffset());
      snippet->resize(viewRect.width(), sHeight);
    }
    else if(snippet->anchor()=="AnchorBottom"){
      //NB: Why do we need to subtract 2 from y coord here???
      //snippet->setPos(0, viewRect.height() - sHeight - snippet->anchorOffset() -2);
      snippet->setPos(0, viewRect.height() - sHeight - snippet->anchorOffset());
      snippet->resize(viewRect.width(), sHeight);
    }
    else if(snippet->anchor()=="AnchorCenter"){
      qreal sWidth = snippet->ownerArea().width();
      snippet->setPos((viewRect.width()-sWidth)/2,(viewRect.height()-sHeight)/2);
      snippet->resize(sWidth, sHeight);
    }
    else if(snippet->anchor()=="AnchorFullScreen"){
      snippet->setRect(0,0,viewRect.width(), viewRect.height());
    }
    snippet->updateChildGeometries();
    
  }
  
  updateViewPort();

  repaintRequested(viewRect); //Do intial repaint of the whole chrome after snippets are inited
}

//Updates the current viewport size to the area not covered by visible top and bottom chrome.

void ChromeWidget::updateViewPort() {
  QRect viewPort(QPoint(0,0), m_parentChromeView->geometry().size());
  
  //NB: Note that this algorithm assumes that anchor offsets do NOT
  //shrink the viewport. I.e., if you have an offset snippet it is
  //assumed either that it hides content (HidesContent attribute is set)
  //or that it is being stacked on another anchored snippet. An offset snippet 
  //that is not being stacked on another snippet and that does not have content hiding
  //set (HidesContent attribute) will typically show on top of the content window
  //with the content window reduced by the size of the snippet.
  int viewPortY = 0;
  foreach(ChromeSnippet *snippet, m_snippetMap) {
    if(!snippet->hidesContent()){
      if((snippet->anchor()=="AnchorTop") && snippet->isVisible() && !snippet->isHiding()){
        int snippetY = snippet->pos().y() + snippet->ownerArea().height();
        if  (snippetY > viewPortY) {
            viewPortY = snippetY;
        }
      }
      else if((snippet->anchor()=="AnchorBottom") && snippet->isVisible() && !snippet->isHiding()){
        viewPort.adjust(0, 0, 0, (int)-snippet->ownerArea().height());
      }
    }
  }
  viewPort.adjust(0, viewPortY, 0, 0);
  emit viewPortResize(viewPort);
}

//Explicitly reset the viewport to a specified rectangle

void ChromeWidget::setViewPort(QRect viewPort){
  emit viewPortResize(viewPort);
}

void ChromeWidget::networkRequestFinished(QNetworkReply *reply){  // slot
  if(reply->error() != QNetworkReply::NoError) {
    //qDebug() << "ChromeWidget::networkRequestFinished: " << reply->errorString();
  }
}

// Called when some part of the chrome page needs repainting.  Uses a custom event to delay calling mainFrame->render()
// since in some cases render() can crash -- apparently when it tries to paint an element that has
// been deleted (by javascript etc.).  Coalesces multiple calls to repaintRequested() into one call to
// paintDirtyRegion().
void ChromeWidget::repaintRequested(QRect dirtyRect){  // slot
  //qDebug() << "ChromeWidget::repaintRequested: " << dirtyRect;

#ifdef G_TIMING
 GTimer * t = new GTimer();
 t->start("ChromeWidget::repaintRequested");
#endif

#ifdef Q_OS_SYMBIANXX // turn off the hack for now, remove eventually
  // Hack to get around painting issue in text fields.  Backspacing doesn't appear to generate
  // repaint requests though the blinking caret does.  Since the caret is very narrow this leaves 
  // behind artifacts of the character that was deleted.
  dirtyRect.setRight(dirtyRect.right() + 20);
  
  //NB:Delayed repaints don't get invoked on NSP, at least on emulator
  //so paint immediately. Note that delayed repaints are a work-around
  //for JS/DOM issues in WebKit, so this needs to be revisited.
  m_dirtyRegion = dirtyRect;
  paintDirtyRegion();
#else
  if(m_dirtyRegion.isEmpty()) {
      m_dirtyRegion += dirtyRect;
      QCoreApplication::postEvent(this, new UpdateBufferEvent);
  }
  else
      m_dirtyRegion += dirtyRect;
#endif
#ifdef G_TIMING
  t->stop();
  t->save();
  delete t;
#endif
}

void ChromeWidget::paintDirtyRegion() {
  //qDebug() << "ChromeWidget::paintDirtyRegion" << m_dirtyRegion;
  
  if(m_dirtyRegion.isEmpty())
      return;
  if(m_buffer){
    m_painter->save(); //NB: would it be more efficient just to create a new painter on the stack?
    //Must set clip rect because frame may render background(?) outside dirty rect
    m_painter->setClipRegion(m_dirtyRegion);
    if(chromePage() && chromePage()->mainFrame())
        chromePage()->mainFrame()->render(m_painter, m_dirtyRegion);
    m_painter->restore();
  }

  foreach(ChromeSnippet *snippet, m_snippetMap) {
    if((snippet->parentItem() == m_parentItem) && snippet->isVisible() && m_dirtyRegion.intersects(snippet->ownerArea().toRect())) {
      //  qDebug() << "Dirty rect intersects: " << snippet->docElementId() << ": " << snippet->ownerArea().toRect();
      snippet->update();
    }
  }

  // Clear dirty region.
  m_dirtyRegion = QRegion();
}


// Update owner areas of all snippets to allow for changes in chrome page geometry.
void ChromeWidget::updateOwnerAreas() {
  foreach(ChromeSnippet *snippet, m_snippetMap) {
    snippet->setOwnerArea(getDocElementRect(snippet->docElementId()));
  }
}

//NB: The following methods should also be implementable, and possibly
//more efficient, via the C++ DOM API

void ChromeWidget::debugAlert(const QString &msg){
  chromePage()->mainFrame()->evaluateJavaScript("alert('" + msg + "')");
}

QVariant ChromeWidget::getDocElement(const QString &id) {
  return chromePage()->mainFrame()->evaluateJavaScript("document.getElementById('" + id + "')");
}

QVariant ChromeWidget::getDocIdsByName(const QString &name){

  QString js (
	      "var elements = document.getElementsByName('" + name + "');"
              "var result = new Array();"
              "for(i = 0 ; i< elements.length; i++){"
              " result[i]=elements[i].id;"
              "}"
              "result;"
	     );
  return chromePage()->mainFrame()->evaluateJavaScript(js);
}

QVariant ChromeWidget::getDocIdsByClassName(const QString &name){

  QString js (
              "var elements = document.getElementsByClassName('" + name + "');"
              "var result = new Array();"
              "for(i = 0 ; i< elements.length; i++){"
              " result[i]=elements[i].id;"
              "}"
              "result;"
             );
  return chromePage()->mainFrame()->evaluateJavaScript(js);
}

QVariant ChromeWidget::getChildIdsByClassName(const QString &parentId, const QString &name){

  QString js (
	      "var elements = document.getElementsByClassName('" + name + "');"
              "var result = new Array();"
              "for(i = 0 ; i< elements.length; i++){"
              "if(elements[i].parentNode.id == '" + parentId +"'){"
              " result[i]=elements[i].id;"
              "}"
              "}"
              "result;"
	     );
  return chromePage()->mainFrame()->evaluateJavaScript(js);

}

QSize ChromeWidget::getDocElementSize(const QString &id) {
  QSize result;
  QVariant jObj = getDocElement(id);
  if(jObj.isValid()) {
      QMap<QString, QVariant> jMap = jObj.toMap();
      //qDebug() << "Tagname: " << (jMap["tagName"].toString());
      result.setWidth(jMap["clientWidth"].toInt());
      result.setHeight(jMap["clientHeight"].toInt());
  }
  else {
    qDebug() << "ChromeWidget::getDocElementSize: element not found. " << id;
  }
  return result;
}

QString ChromeWidget::getDocElementAttribute(const QString &id, const QString &attribute) {
  QString result;
  QVariant jObj = getDocElement(id);
  if(jObj.isValid()) {
      QMap<QString, QVariant> jMap = jObj.toMap();
      //qDebug() << "Tagname: " << (jMap["tagName"].toString());
      result = jMap[attribute].toString();
  }
  else {
    qDebug() << "ChromeWidget::getDocElementSize: element not found. " << id;
  }
  return result;
}

QRect ChromeWidget::getDocElementRect(const QString &id) {
  QString js("var obj = document.getElementById('" + id + "');"
             "var width = obj.clientWidth;"
             "var height = obj.clientHeight;"
             "var curleft = curtop = 0;"
             "do {"
             "  curleft += obj.offsetLeft;"
             "  curtop += obj.offsetTop;"
             "} while (obj = obj.offsetParent);"
             "[curleft, curtop, width, height]");
  QVariant jObj = chromePage()->mainFrame()->evaluateJavaScript(js);
  if(jObj.isValid()) {
    return QRect(jObj.toList()[0].toInt(), jObj.toList()[1].toInt(), jObj.toList()[2].toInt(), jObj.toList()[3].toInt());
  }
  else {
    qDebug() << "ChromeWidget::getDocElementRect: element not found. " << id;
    return QRect();
  }
}

// Private.  This class shadows the Qt class QComboBoxPrivateContainer to provide access its
// the 'combo' pointer in eventFilter().
class xQComboBoxPrivateContainer : public QFrame
{
  public:
    int spacing() const;
    QTimer blockMouseReleaseTimer;
    QBasicTimer adjustSizeTimer;
    QPoint initialClickPosition;
    QComboBox *combo;
    QAbstractItemView *view;
    void *top;
    void *bottom;
};

bool ChromeWidget::eventFilter(QObject *object, QEvent *event)
{
    // Shameless hack here.  We need to intercept the creation of combobox drop-downs
    // in the chrome and move them into their correct positions since the system thinks they belong
    // off-screen over where the chrome page is actually rendered.  Since drop-downs are grandchildren
    // of the ChromeRenderer we start by watching for child added events, when one is created we
    // watch it also for child added events too, thereby watching grandchild events.  When we
    // see a QComboBoxPrivateContainer (the drop-down list) being moved we move it instead into
    // position just under the combobox.

    //qDebug() << "ChromeWidget::eventFilter: " << event->type();

    switch ((int)event->type()) {
      case QEvent::ChildAdded:
      case QEvent::ChildPolished:
      {
        QChildEvent *childEvt = static_cast<QChildEvent *>(event);
        //qDebug() << "    watching " << childEvt->child();
        childEvt->child()->installEventFilter(this);
        break;
      }
      case QEvent::Move:
      {
        //QMoveEvent *evt = static_cast<QMoveEvent *>(event);
        //qDebug() << "    oldpos " << evt->oldPos() << " pos " << evt->pos();
        if(object->inherits("QComboBoxPrivateContainer")) {
            xQComboBoxPrivateContainer *cbpc = static_cast<xQComboBoxPrivateContainer *>(object);
            QComboBox *combo = cbpc->combo;
            QRect comboRect = combo->geometry();
            QPoint comboPos = comboRect.topLeft();
            ChromeSnippet *snippet = getSnippet(comboPos);
            if(snippet) {
                QPoint relativePos = comboPos - snippet->ownerArea().topLeft().toPoint();
                static_cast<QWidget *>(object)->move(m_parentChromeView->mapToGlobal(QPoint(0,0))
                                                     + snippet->rect().topLeft().toPoint()
                                                     + relativePos
                                                     + QPoint(0, comboRect.height()));
            }
        }
        break;
      }
      default:
      {
        if(event->type() == UpdateBufferEvent::customType()) {
            if(object == this) {
                //qDebug() << "ChromeWidget::eventFilter: UpdateBufferEvent " << (void*)object << event;
                paintDirtyRegion();
            }
        }
        break;
      }
    }

    return QObject::eventFilter(object, event);
}

ChromeSnippet *ChromeWidget::getSnippet(QPoint pos) const {
    foreach(ChromeSnippet *snippet, m_snippetMap) {
        if(snippet->ownerArea().contains(pos))
            return snippet;
    }
    return 0;
}

void ChromeWidget::dump() {
    qDebug() << "ChromeWidget::dump";
    foreach(ChromeSnippet *snippet, m_snippetMap) {
        snippet->dump();
        qDebug() << "------";
    }
}