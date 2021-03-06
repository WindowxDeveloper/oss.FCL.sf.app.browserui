/**
   This file is part of CWRT package **

   Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies). **

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU (Lesser) General Public License as 
   published by the Free Software Foundation, version 2.1 of the License. 
   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of 
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
   (Lesser) General Public License for more details. You should have 
   received a copy of the GNU (Lesser) General Public License along 
   with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DOWNLOADFACTORY_H_
#define DOWNLOADFACTORY_H_

class DownloadBackend;
class DownloadCore;
class ClientDownload;

// class declaration

// responsible for creating concrete download implementation class
// based on the content type
class DownloadAbstractFactory
{
public:
    static DownloadBackend* createDownloadImplementation(DownloadCore *dlCore, ClientDownload *dl);
};



#endif
