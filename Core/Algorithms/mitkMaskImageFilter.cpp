/*=========================================================================

Program:   Medical Imaging & Interaction Toolkit
Module:    $RCSfile$
Language:  C++
Date:      $Date$
Version:   $Revision$

Copyright (c) German Cancer Research Center, Division of Medical and
Biological Informatics. All rights reserved.
See MITKCopyright.txt or http://www.mitk.org/copyright.html for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notices for more information.

=========================================================================*/


#include "mitkMaskImageFilter.h"
#include "mitkImageTimeSelector.h"
#include "mitkTimeHelper.h"
#include "mitkProperties.h"

mitk::MaskImageFilter::MaskImageFilter() : m_Mask(NULL)
{
  this->SetNumberOfInputs(2);
  this->SetNumberOfRequiredInputs(2);
  m_InputTimeSelector  = mitk::ImageTimeSelector::New();
  m_MaskTimeSelector   = mitk::ImageTimeSelector::New();
  m_OutputTimeSelector = mitk::ImageTimeSelector::New();
}

mitk::MaskImageFilter::~MaskImageFilter()
{

}

void mitk::MaskImageFilter::SetMask( const mitk::Image* mask ) 
{
	// Process object is not const-correct so the const_cast is required here
  m_Mask = const_cast< mitk::Image * >( mask );
	this->ProcessObject::SetNthInput(1, m_Mask );
}

const mitk::Image* mitk::MaskImageFilter::GetMask() const 
{
  return m_Mask;
}

void mitk::MaskImageFilter::GenerateInputRequestedRegion()
{
  Superclass::GenerateInputRequestedRegion();

  mitk::Image* output = this->GetOutput();
  mitk::Image* input = const_cast< mitk::Image * > ( this->GetInput() );
  mitk::Image* mask  = m_Mask ;
  if((output->IsInitialized()==false) || (mask == NULL) || (mask->GetTimeSlicedGeometry()->GetTimeSteps() == 0))
    return;

  input->SetRequestedRegionToLargestPossibleRegion();
  mask->SetRequestedRegionToLargestPossibleRegion();

  GenerateTimeInInputRegion(output, input);
  GenerateTimeInInputRegion(output, mask);
}

void mitk::MaskImageFilter::GenerateOutputInformation()
{
  mitk::Image::ConstPointer input = this->GetInput();
  mitk::Image::Pointer output = this->GetOutput();

  if ((output->IsInitialized()) && (this->GetMTime() <= m_TimeOfHeaderInitialization.GetMTime()))
    return;

  itkDebugMacro(<<"GenerateOutputInformation()");

  unsigned int i;
  unsigned int *tmpDimensions = new unsigned int[input->GetDimension()];

  for(i=0;i<input->GetDimension();++i)
    tmpDimensions[i]=input->GetDimension(i);

  output->Initialize(input->GetPixelType(),
    input->GetDimension(),
    tmpDimensions,
    input->GetNumberOfChannels());

  delete [] tmpDimensions;

  output->SetGeometry(static_cast<mitk::Geometry3D*>(input->GetGeometry()->Clone().GetPointer()));

  output->SetPropertyList(input->GetPropertyList()->Clone());    

  m_TimeOfHeaderInitialization.Modified();
}

template < typename TPixel, unsigned int VImageDimension >
void mitk::_InternalComputeMask(itk::Image<TPixel, VImageDimension>* inputItkImage, mitk::MaskImageFilter* MaskImageFilter)
{
  typedef itk::Image<TPixel, VImageDimension> ItkInputImageType;

  typedef itk::Image<unsigned char, VImageDimension> ItkMaskImageType;
  typedef itk::Image<TPixel, VImageDimension> ItkOutputImageType;

  typedef itk::ImageRegionConstIterator< ItkInputImageType > ItkInputImageIteratorType;
  typedef itk::ImageRegionConstIterator< ItkMaskImageType > ItkMaskImageIteratorType;
  typedef itk::ImageRegionIteratorWithIndex< ItkOutputImageType > ItkOutputImageIteratorType;

  typename mitk::ImageToItk<ItkMaskImageType>::Pointer maskimagetoitk = mitk::ImageToItk<ItkMaskImageType>::New();
  maskimagetoitk->SetInput(MaskImageFilter->m_MaskTimeSelector->GetOutput());
  maskimagetoitk->Update();
  typename ItkMaskImageType::Pointer maskItkImage = maskimagetoitk->GetOutput();

  typename mitk::ImageToItk<ItkOutputImageType>::Pointer outputimagetoitk = mitk::ImageToItk<ItkOutputImageType>::New();
  outputimagetoitk->SetInput(MaskImageFilter->m_OutputTimeSelector->GetOutput());
  outputimagetoitk->Update();
  typename ItkOutputImageType::Pointer outputItkImage = outputimagetoitk->GetOutput();

  // create the iterators
  ItkInputImageType::RegionType inputRegionOfInterest = inputItkImage->GetLargestPossibleRegion();
  ItkInputImageIteratorType  inputIt( inputItkImage, inputRegionOfInterest );
  ItkMaskImageIteratorType  maskIt ( maskItkImage, inputRegionOfInterest );
  ItkOutputImageIteratorType outputIt( outputItkImage, inputRegionOfInterest );

  typename ItkOutputImageType::PixelType m_OutsideValue = itk::NumericTraits<typename ItkOutputImageType::PixelType>::min();

  for ( inputIt.GoToBegin(), maskIt.GoToBegin(), outputIt.GoToBegin(); !inputIt.IsAtEnd() && !maskIt.IsAtEnd(); ++inputIt, ++maskIt, ++outputIt)
  {
    if ( maskIt.Get() > itk::NumericTraits<typename ItkMaskImageType::PixelType>::Zero )
    {
      outputIt.Set(inputIt.Get());
    }
    else
    {
      outputIt.Set(m_OutsideValue);
    }
  }
}

#include "mitkImageAccessByItk.h"

void mitk::MaskImageFilter::GenerateData()
{
  mitk::Image::ConstPointer input = this->GetInput();
  mitk::Image::ConstPointer mask  = m_Mask;
  mitk::Image::Pointer output = this->GetOutput();

  if((output->IsInitialized()==false) || (mask.IsNull()) || (mask->GetTimeSlicedGeometry()->GetTimeSteps() == 0))
    return;

  m_InputTimeSelector->SetInput(input);
  m_MaskTimeSelector->SetInput(mask);
  m_OutputTimeSelector->SetInput(this->GetOutput());

  mitk::Image::RegionType outputRegion = output->GetRequestedRegion();
  const mitk::TimeSlicedGeometry *outputTimeGeometry = output->GetTimeSlicedGeometry();
  const mitk::TimeSlicedGeometry *inputTimeGeometry = input->GetTimeSlicedGeometry();
  const mitk::TimeSlicedGeometry *maskTimeGeometry = mask->GetTimeSlicedGeometry();
  ScalarType timeInMS;

  int timestep=0;
  int tstart=outputRegion.GetIndex(3);
  int tmax=tstart+outputRegion.GetSize(3);

  int t;
  for(t=tstart;t<tmax;++t)
  {
    timeInMS = outputTimeGeometry->TimeStepToMS( t );

    timestep = inputTimeGeometry->MSToTimeStep( timeInMS );

    m_InputTimeSelector->SetTimeNr(timestep);
    m_InputTimeSelector->UpdateLargestPossibleRegion();
    m_OutputTimeSelector->SetTimeNr(t);
    m_OutputTimeSelector->UpdateLargestPossibleRegion();

    timestep = maskTimeGeometry->MSToTimeStep( timeInMS );
    m_MaskTimeSelector->SetTimeNr(timestep);
    m_MaskTimeSelector->UpdateLargestPossibleRegion();

    AccessByItk_1(m_InputTimeSelector->GetOutput(),_InternalComputeMask,this);
  }

  m_TimeOfHeaderInitialization.Modified();
}
