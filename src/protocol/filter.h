// Copyright (c) 2014-2017 Coin Sciences Ltd
// MultiChain code distributed under the GPLv3 license, see COPYING file.

#ifndef MULTICHAIN_FILTER_H
#define MULTICHAIN_FILTER_H

#include "utils/declare.h"
#include "utils/util.h"



typedef struct mc_Filter
{    
    void *m_Impl;
    
    mc_Filter()
    {
        Zero();
    }
    
    ~mc_Filter()
    {
        Destroy();
    }
    
    
    int Zero();
    int Destroy();   
} mc_Filter;

typedef struct mc_FilterEngine
{    
    mc_FilterEngine()
    {
        Zero();
    }
    
    ~mc_FilterEngine()
    {
        Destroy();
    }
    

    int Initialize();
    
    int CreateFilter(const char *script,const char* main_name,mc_Filter *filter,std::string &strResult);
    int RunFilter(const mc_Filter& filter,std::string &strResult);        
    
    int Zero();
    int Destroy();
    
} mc_FilterEngine;

#endif /* MULTICHAIN_FILTER_H */