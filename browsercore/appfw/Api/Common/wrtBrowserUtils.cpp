/*
* Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
* All rights reserved.
* This component and the accompanying materials are made available
* under the terms of "Eclipse Public License v1.0"
* which accompanies this distribution, and is available
* at the URL "http://www.eclipse.org/legal/epl-v10.html".
*
* Initial Contributors:
* Nokia Corporation - initial contribution.
*
* Contributors:
*
* Description: 
*
*/


#include "wrtBrowserUtils.h"


wrtBrowserUtils::wrtBrowserUtils()
{
}

wrtBrowserUtils::~wrtBrowserUtils()
{
}
wrtBrowserDefs::BrowserElementType wrtBrowserUtils::ElementType(QWebElement* node)
{
	if( !node || node->isNull())
        return wrtBrowserDefs::EElementNone;

    wrtBrowserDefs::BrowserElementType elType(wrtBrowserDefs::EElementNone);
   
    // get the right element type
	if ( !node->tagName().compare(LINK_TAG,Qt::CaseInsensitive) ){
        elType = wrtBrowserDefs::EElementAnchor;
    }
	if ( !node->tagName().compare(INPUT_TAG,Qt::CaseInsensitive) ){
        elType = wrtBrowserDefs::EElementInputBox;
    }
	else if ( !node->tagName().compare(A_TAG,Qt::CaseInsensitive) ){
		elType = wrtBrowserDefs::EElementAnchor;        
        QString href = node->attribute( HREF_ATTR );
		if( !node->tagName().compare(AREA_TAG,Qt::CaseInsensitive) ){
			elType = wrtBrowserDefs::EElementAreaBox;                        
        }
    }
	if (!node->tagName().compare(TEXT_AREA_TAG,Qt::CaseInsensitive))
        elType = wrtBrowserDefs::EElementTextAreaBox;  
	if (!node->tagName().compare(SELECT_TAG,Qt::CaseInsensitive))
        elType = wrtBrowserDefs::EElementSelectBox;  
	if (!node->tagName().compare(BUTTON_TAG,Qt::CaseInsensitive))
        elType = wrtBrowserDefs::EElementButton; 
	if (!node->tagName().compare(FORM_TAG,Qt::CaseInsensitive))
        elType = wrtBrowserDefs::EElementForm;  
	if (!node->tagName().compare(LI_TAG,Qt::CaseInsensitive)||!node->tagName().compare(OL_TAG,Qt::CaseInsensitive)||!node->tagName().compare(DL_TAG,Qt::CaseInsensitive))
        elType = wrtBrowserDefs::EElementListBox;  
	else if( !node->tagName().compare(OBJECT_TAG,Qt::CaseInsensitive) || !node->tagName().compare(EMBED_TAG,Qt::CaseInsensitive) ){
		elType = wrtBrowserDefs::EElementObjectBox;        
    }
    return elType;
}

wrtBrowserDefs::BrowserElementType wrtBrowserUtils::getTypeFromElement(QWebHitTestResult htRes) 
{
	QWebElement* node = &htRes.element();
	wrtBrowserDefs::BrowserElementType elType = wrtBrowserDefs::EElementNone;
	if(node){
		
			if(node->localName() == HTML_TAG){
				elType = wrtBrowserDefs::EElementNone;
			}
			else if(node->tagName().isEmpty()){
				//check whether hit test returns a navigatable element				
				if(!htRes.linkElement().isNull()){
					elType = wrtBrowserDefs::EElementAnchor;
				}
			}
			else if (!node->tagName().compare(AREA_TAG,Qt::CaseInsensitive)){
				if(!node->attribute(HREF_ATTR).isNull()){
                        elType = wrtBrowserDefs::EElementAreaBox;
                    }
				else{
                        elType = wrtBrowserDefs::EElementAnchor;
				}
            }
			else if (!node->tagName().compare(IMAGE_TAG,Qt::CaseInsensitive)){
				 elType = wrtBrowserDefs::EElementImageBox;
				 if(!htRes.linkElement().isNull()){
				        elType = wrtBrowserDefs::EElementAnchor;
				 }
             }
			 else{
				 elType = ElementType(node);
             }
	}
	return elType;
}