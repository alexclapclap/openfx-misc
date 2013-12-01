/*
OFX TimeOffset plugin.
Move the input clip forward or backward in time.
This can also reverse the order of the input frames so that last one is first.

Copyright (C) 2013 INRIA
Author Frederic Devernay frederic.devernay@inria.fr

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

  Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

  Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

  Neither the name of the {organization} nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

INRIA
Domaine de Voluceau
Rocquencourt - B.P. 105
78153 Le Chesnay Cedex - France


The skeleton for this source file is from:
OFX Basic Example plugin, a plugin that illustrates the use of the OFX Support library.

Copyright (C) 2004-2005 The Open Effects Association Ltd
Author Bruno Nicoletti bruno@thefoundry.co.uk

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
* Neither the name The Open Effects Association Ltd, nor the names of its 
contributors may be used to endorse or promote products derived from this
software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The Open Effects Association Ltd
1 Wardour St
London W1D 6PA
England

*/

#include <stdio.h>
#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"

#include "../include/ofxsProcessing.H"

namespace OFX {
  extern ImageEffectHostDescription gHostDescription;
}

// Base class for the RGBA and the Alpha processor
class CopierBase : public OFX::ImageProcessor {
protected :
  OFX::Image *_srcImg;
public :
  /** @brief no arg ctor */
  CopierBase(OFX::ImageEffect &instance)
    : OFX::ImageProcessor(instance)
    , _srcImg(0)
  {        
  }

  /** @brief set the src image */
  void setSrcImg(OFX::Image *v) {_srcImg = v;}
};

// template to do the RGBA processing
template <class PIX, int nComponents>
class ImageCopier : public CopierBase {
public :
  // ctor
  ImageCopier(OFX::ImageEffect &instance) 
    : CopierBase(instance)
  {}

  // and do some processing
  void multiThreadProcessImages(OfxRectI procWindow)
  {
    for(int y = procWindow.y1; y < procWindow.y2; y++) {
      if(_effect.abort()) break;

      PIX *dstPix = (PIX *) _dstImg->getPixelAddress(procWindow.x1, y);

      for(int x = procWindow.x1; x < procWindow.x2; x++) {

        PIX *srcPix = (PIX *)  (_srcImg ? _srcImg->getPixelAddress(x, y) : 0);

        // do we have a source image to scale up
        if(srcPix) {
          for(int c = 0; c < nComponents; c++) {
            dstPix[c] = srcPix[c];
          }
        }
        else {
          // no src pixel here, be black and transparent
          for(int c = 0; c < nComponents; c++) {
            dstPix[c] = 0;
          }
        }

        // increment the dst pixel
        dstPix += nComponents;
      }
    }
  }
};


////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class TimeOffsetPlugin : public OFX::ImageEffect {
protected :
    // do not need to delete these, the ImageEffect is managing them for us
    OFX::Clip *dstClip_;            /**< @brief Mandated output clips */
    OFX::Clip *srcClip_;            /**< @brief Mandated input clips */

    OFX::IntParam  *time_offset_;      /**< @brief only used in the filter context. */
    OFX::BooleanParam  *reverse_input_;

public :
    /** @brief ctor */
    TimeOffsetPlugin(OfxImageEffectHandle handle)
      : ImageEffect(handle)
      , dstClip_(0)
      , srcClip_(0)
      , time_offset_(0)
      , reverse_input_(0)
    {
        dstClip_ = fetchClip(kOfxImageEffectOutputClipName);
        srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);

        time_offset_   = fetchIntParam("time_offset");
        reverse_input_ = fetchBooleanParam("reverse_input");
    }

    /* Override the render */
    virtual void render(const OFX::RenderArguments &args);

    /* override the time domain action, only for the general context */
    virtual bool getTimeDomain(OfxRangeD &range);
    
    /* set up and run a processor */
    void
    setupAndProcess(CopierBase &, const OFX::RenderArguments &args);
};


////////////////////////////////////////////////////////////////////////////////
/** @brief render for the filter */

////////////////////////////////////////////////////////////////////////////////
// basic plugin render function, just a skelington to instantiate templates from

// make sure components are sane
static void
checkComponents(const OFX::Image &src,
                OFX::BitDepthEnum dstBitDepth,
                OFX::PixelComponentEnum dstComponents)
{
    OFX::BitDepthEnum      srcBitDepth     = src.getPixelDepth();
    OFX::PixelComponentEnum srcComponents  = src.getPixelComponents();
        
    // see if they have the same depths and bytes and all
    if(srcBitDepth != dstBitDepth || srcComponents != dstComponents)
        throw int(1); // HACK!! need to throw an sensible exception here!        
}

/* set up and run a processor */
void
TimeOffsetPlugin::setupAndProcess(CopierBase &processor, const OFX::RenderArguments &args)
{
    // get a dst image
    std::auto_ptr<OFX::Image>  dst(dstClip_->fetchImage(args.time));
    OFX::BitDepthEnum          dstBitDepth    = dst->getPixelDepth();
    OFX::PixelComponentEnum    dstComponents  = dst->getPixelComponents();
  
    // figure the frame we should be retiming from
    double sourceTime = args.time + time_offset_->getValue(); // no animation
    OfxRangeD range = srcClip_->getFrameRange();
    bool reverse_input = reverse_input_->getValue();
    if (reverse_input) {
      sourceTime = range.max - sourceTime + range.min;
    }
    // clip to min/max range
    if (sourceTime < range.min) {
      sourceTime = range.min;
    } else if (sourceTime > range.max) {
      sourceTime = range.max;
    }
    // fetch the source image
    std::auto_ptr<OFX::Image> srcImg(srcClip_->fetchImage(sourceTime));

    // make sure bit depths are sane
    if(srcImg.get()) checkComponents(*srcImg, dstBitDepth, dstComponents);

    // set the images
    processor.setDstImg(dst.get());
    processor.setSrcImg(srcImg.get());

    // set the render window
    processor.setRenderWindow(args.renderWindow);

    // Call the base class process member, this will call the derived templated process code
    processor.process();
}

/* override the time domain action, only for the general context */
bool 
TimeOffsetPlugin::getTimeDomain(OfxRangeD &range)
{
    // this should only be called in the general context, ever!
    if(getContext() == OFX::eContextGeneral) {
        int time_offset = time_offset_->getValue(); //don't animate
        
        // how many frames on the input clip
        OfxRangeD srcRange = srcClip_->getFrameRange();
        
        range.min = srcRange.min + time_offset;
        range.max = srcRange.max + time_offset;
        return true;
    }

    return false;
}

// the overridden render function
void
TimeOffsetPlugin::render(const OFX::RenderArguments &args)
{
    // instantiate the render code based on the pixel depth of the dst clip
    OFX::BitDepthEnum       dstBitDepth    = dstClip_->getPixelDepth();
    OFX::PixelComponentEnum dstComponents  = dstClip_->getPixelComponents();

    // do the rendering
    if(dstComponents == OFX::ePixelComponentRGBA) {
        switch(dstBitDepth) {
        case OFX::eBitDepthUByte : {      
            ImageCopier<unsigned char, 4> fred(*this);
            setupAndProcess(fred, args);
        }
        break;

        case OFX::eBitDepthUShort : {
            ImageCopier<unsigned short, 4> fred(*this);
            setupAndProcess(fred, args);
        }                          
        break;

        case OFX::eBitDepthFloat : {
            ImageCopier<float, 4> fred(*this);
            setupAndProcess(fred, args);
        }
        break;
        default :
            OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    }
    else {
        switch(dstBitDepth) {
        case OFX::eBitDepthUByte : {
            ImageCopier<unsigned char, 1> fred(*this);
            setupAndProcess(fred, args);
        }
        break;

        case OFX::eBitDepthUShort : {
            ImageCopier<unsigned short, 1> fred(*this);
            setupAndProcess(fred, args);
        }                          
        break;

        case OFX::eBitDepthFloat : {
            ImageCopier<float, 1> fred(*this);
            setupAndProcess(fred, args);
        }                          
        break;
        default :
            OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    } // switch
}

using namespace OFX;
mDeclarePluginFactory(TimeOffsetPluginFactory, ;, {});

namespace OFX
{
  namespace Plugin
  {
    void getPluginIDs(OFX::PluginFactoryArray &ids)
    {
      static TimeOffsetPluginFactory p("net.sf.openfx:timeOffset", 1, 0);
      ids.push_back(&p);
    }
  };
};

void TimeOffsetPluginFactory::load()
{
  // we can't be used on hosts that don't perfrom temporal clip access
  if(!gHostDescription.temporalClipAccess) {
    throw OFX::Exception::HostInadequate("Need random temporal image access to work");
  }
}

/** @brief The basic describe function, passed a plugin descriptor */
void TimeOffsetPluginFactory::describe(OFX::ImageEffectDescriptor &desc) 
{
  // basic labels
  desc.setLabels("TimeOffsetOFX", "TimeOffsetOFX", "TimeOffsetOFX");
  desc.setPluginGrouping("OFX/Time");
  desc.setPluginDescription("Move the input clip forward or backward in time. "
                            "This can also reverse the order of the input frames so that last one is first.");

  // Say we are a filer context
  desc.addSupportedContext(OFX::eContextFilter);
  desc.addSupportedContext(OFX::eContextGeneral);

  // Add supported pixel depths
  desc.addSupportedBitDepth(eBitDepthUByte);
  desc.addSupportedBitDepth(eBitDepthUShort);
  desc.addSupportedBitDepth(eBitDepthFloat);

  // set a few flags
  desc.setSingleInstance(false);
  desc.setHostFrameThreading(false);
  desc.setSupportsMultiResolution(true);
  desc.setSupportsTiles(true);
  desc.setTemporalClipAccess(true); // say we will be doing random time access on clips
  desc.setRenderTwiceAlways(false);
  desc.setSupportsMultipleClipPARs(false);

  // we can't be used on hosts that don't perfrom temporal clip access
  if(!gHostDescription.temporalClipAccess) {
    throw OFX::Exception::HostInadequate("Need random temporal image access to work");
  }
}        

/** @brief The describe in context function, passed a plugin descriptor and a context */
void TimeOffsetPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc, ContextEnum context) 
{
  // we are a transition, so define the sourceTo input clip
  ClipDescriptor *toClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
  toClip->addSupportedComponent(ePixelComponentRGBA);
  toClip->addSupportedComponent(ePixelComponentAlpha);
  toClip->setTemporalClipAccess(true); // say we will be doing random time access on this clip
  toClip->setSupportsTiles(true);
  toClip->setFieldExtraction(eFieldExtractDoubled); // which is the default anyway

  // create the mandated output clip
  ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
  dstClip->addSupportedComponent(ePixelComponentRGBA);
  dstClip->addSupportedComponent(ePixelComponentAlpha);
  toClip->setFieldExtraction(eFieldExtractDoubled); // which is the default anyway
  dstClip->setSupportsTiles(true);

  // make some pages and to things in 
  PageParamDescriptor *page = desc.definePageParam("Controls");

  IntParamDescriptor *time_offset = desc.defineIntParam("time_offset");
  time_offset->setLabels("time offset (frames)", "time offset (frames)", "time offset (frames)");
  time_offset->setScriptName("time_offset");
  time_offset->setHint("Offset in frames (frame f from the input will be at f+offset)");
  time_offset->setDefault(0);
  // keep default range (INT_MIN..INT_MAX)
  // keep default display range (same as range)
  time_offset->setAnimates(false); // no animation here!

  page->addChild(*time_offset);

  BooleanParamDescriptor *reverse_input = desc.defineBooleanParam("reverse_input");
  reverse_input->setDefault(false);
  reverse_input->setHint("Reverse the order of the input frames so that last one is first");
  reverse_input->setLabels("reverse input", "reverse input", "reverse input");
  reverse_input->setAnimates(false); // no animation here!

  page->addChild(*reverse_input);

}

/** @brief The create instance function, the plugin must return an object derived from the \ref OFX::ImageEffect class */
ImageEffect* TimeOffsetPluginFactory::createInstance(OfxImageEffectHandle handle, ContextEnum context)
{
  return new TimeOffsetPlugin(handle);
}