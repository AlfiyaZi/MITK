#ifndef MITKBOUNDINGOBJECTCUTTER_TXX
#define MITKBOUNDINGOBJECTCUTTER_TXX

#include "mitkBoundingObjectCutter.h"
#include "mitkImageToItkMultiplexer.h"
#include "mitkImage.h"
#include "mitkBoundingObject.h"
#include "mitkGeometry3D.h"
#include <mitkStatusBar.h>
#include <vtkTransform.h>

namespace mitk
{

template <typename TPixel>
BoundingObjectCutter<TPixel>::BoundingObjectCutter()
  : m_UseInsideValue(false), m_OutsideValue(0), m_InsideValue(1), m_BoundingObject(NULL), 
  m_OutsidePixelCount(0), m_InsidePixelCount(0)
{
}

template <typename TPixel>
BoundingObjectCutter<TPixel>::~BoundingObjectCutter()
{
}

/**
 * \todo check if this is no conflict to the ITK filter writing rules -> ITK SoftwareGuide p.512
 */
template <typename TPixel>
void BoundingObjectCutter<TPixel>::GenerateOutputInformation()
{
  itkDebugMacro(<<"GenerateOutputInformation()");
}

template <typename TPixel>
void BoundingObjectCutter<TPixel>::GenerateData()
{
  /* Get pointer to output of filter */
  Image::Pointer outputImage = this->GetOutput();
 
  /* Check the input data */
  if(m_BoundingObject.IsNull())
  {
    outputImage = NULL;
    return;  // return empty image/source image?
  }
  ImageToImageFilter::InputImageConstPointer inputImageMitkPointer = this->GetInput();
  if(inputImageMitkPointer.IsNull())
  {
    outputImage = NULL;
    return;  // Eigentlich: eigenes Spacing/dimensions festlegen und neues Bin�rbild erzeugen
  }
  /* convert input mitk image to itk image */
  Image::Pointer inputImageMitk = const_cast<mitk::Image*>(inputImageMitkPointer.GetPointer());  // const away cast, because FixedInput...Multiplexer Macro needs non const Pointer
  ItkImageType::Pointer itkImage = ItkImageType::New();  
  CastToItkImage(inputImageMitk, itkImage);
  /* check if image conversion failed */
  if (itkImage.IsNull())
  {
    mitk::StatusBar::DisplayErrorText ("An internal error occurred. Can't convert Image. Please report to bugs@mitk.org");
    std::cout << " image is NULL...returning" << std::endl;
    outputImage = NULL; 
    return; // return and do nothing in case of failure
  }
  /* calculate region of interest */
  m_BoundingObject->UpdateOutputInformation();
  
  mitk::BoundingBox::Pointer boundingBox = const_cast<mitk::BoundingBox*>(m_BoundingObject->GetGeometry()->GetBoundingBox());  // const away cast because boundingbox->GetMinimum() is not declared const
  /* get BoundingBox min max and center in world coordinates */
  mitk::BoundingBox::PointType minPoint = boundingBox->GetMinimum();
  mitk::ScalarType min[3];
  min[0] = minPoint[0];   min[1] = minPoint[1];   min[2] = minPoint[2];
  m_BoundingObject->GetGeometry()->GetVtkTransform()->TransformPoint(min, min);   // transform vertex point to world coordinates
  minPoint[0] = min[0];   minPoint[1] = min[1];   minPoint[2] = min[2];
  inputImageMitk->GetGeometry()->MMToUnits(minPoint, minPoint);                             // transform vertex into image unit coordinates
  min[0] = minPoint[0];   min[1] = minPoint[1];   min[2] = minPoint[2];

  mitk::Point3D globalMinPoint;
  mitk::Point3D globalMaxPoint;
  globalMinPoint[0] = min[0];   globalMinPoint[1] = min[1];   globalMinPoint[2] = min[2];  
  globalMaxPoint[0] = min[0];   globalMaxPoint[1] = min[1];   globalMaxPoint[2] = min[2];
  /* create all 8 points of the bounding box */
  mitk::BoundingBox::PointsContainerPointer points = mitk::BoundingBox::PointsContainer::New();
  mitk::Point3D p;
  p = boundingBox->GetMinimum();
  points->InsertElement(0, p);
  p[0] = -p[0];
  points->InsertElement(1, p);
  p = boundingBox->GetMinimum();
  p[1] = -p[1];    
  points->InsertElement(2, p);
  p = boundingBox->GetMinimum();
  p[2] = -p[2];    
  points->InsertElement(3, p);
  p = boundingBox->GetMaximum();
  points->InsertElement(4, p);
  p[0] = -p[0];
  points->InsertElement(5, p);
  p = boundingBox->GetMaximum();
  p[1] = -p[1];    
  points->InsertElement(6, p);
  p = boundingBox->GetMaximum();
  p[2] = -p[2];    
  points->InsertElement(7, p);
  mitk::BoundingBox::PointsContainerConstIterator pointsIterator = points->Begin();
  mitk::BoundingBox::PointsContainerConstIterator pointsIteratorEnd = points->End();
  while (pointsIterator != pointsIteratorEnd) // for each vertex of the bounding box
  {
    minPoint = pointsIterator->Value();
    min[0] = minPoint[0];   min[1] = minPoint[1];   min[2] = minPoint[2];
    m_BoundingObject->GetGeometry()->GetVtkTransform()->TransformPoint(min, min);   // transform vertex point to world coordinates
    minPoint[0] = min[0];   minPoint[1] = min[1];   minPoint[2] = min[2];
    inputImageMitk->GetGeometry()->MMToUnits(minPoint, minPoint);                             // transform vertex into image unit coordinates

    globalMinPoint[0] = (min[0] < globalMinPoint[0]) ? min[0] : globalMinPoint[0];  // check if world point
    globalMinPoint[1] = (min[1] < globalMinPoint[1]) ? min[1] : globalMinPoint[1];  // has a lower or a
    globalMinPoint[2] = (min[2] < globalMinPoint[2]) ? min[2] : globalMinPoint[2];  // higher value as
    globalMaxPoint[0] = (min[0] > globalMaxPoint[0]) ? min[0] : globalMaxPoint[0];  // the last known highest
    globalMaxPoint[1] = (min[1] > globalMaxPoint[1]) ? min[1] : globalMaxPoint[1];  // value
    globalMaxPoint[2] = (min[2] > globalMaxPoint[2]) ? min[2] : globalMaxPoint[2];  // in each axis
    pointsIterator++;
  }
  std::cout << " global bounding box in image coordinates: " << globalMinPoint << " - " << globalMaxPoint<< std::endl;
  /* crop global bounding box to the source image bounding box. @TODO: Is that needed? Or is the regionofinterest.Crop() call below enough? */
/*  mitk::BoundingBox::Pointer inputImageBoundingBox = const_cast<mitk::BoundingBox*>(inputImageMitk->GetGeometry()->GetBoundingBox());
  mitk::BoundingBox::PointType imageMinPoint = inputImageBoundingBox->GetMinimum();
  mitk::BoundingBox::PointType imageMaxPoint = inputImageBoundingBox->GetMaximum();
  globalMinPoint[0] = (globalMinPoint[0] < imageMinPoint[0]) ? imageMinPoint[0] : globalMinPoint[0];
  globalMinPoint[1] = (globalMinPoint[1] < imageMinPoint[1]) ? imageMinPoint[1] : globalMinPoint[1];
  globalMinPoint[2] = (globalMinPoint[2] < imageMinPoint[2]) ? imageMinPoint[2] : globalMinPoint[2];
  globalMaxPoint[0] = (globalMaxPoint[0] > imageMaxPoint[0]) ? imageMaxPoint[0] : globalMaxPoint[0];
  globalMaxPoint[1] = (globalMaxPoint[1] > imageMaxPoint[1]) ? imageMaxPoint[1] : globalMaxPoint[1];
  globalMaxPoint[2] = (globalMaxPoint[2] > imageMaxPoint[2]) ? imageMaxPoint[2] : globalMaxPoint[2];
*/
  /* calculate reg�on of interest in pixel values */
  ItkRegionType regionOfInterest;
  typename ItkImageType::IndexType start;
//  itkImage->TransformPhysicalPointToIndex(globalMinPoint, start);
  start[0] = globalMinPoint[0]; start[1] = globalMinPoint[1]; start[2] = globalMinPoint[2];
  regionOfInterest.SetIndex(start);
  typename ItkImageType::SizeType size;  
  size[0] = static_cast<typename ItkImageType::SizeType::SizeValueType>((globalMaxPoint[0] - globalMinPoint[0])/ itkImage->GetSpacing()[0]); // number of pixels along X axis
  size[1] = static_cast<typename ItkImageType::SizeType::SizeValueType>((globalMaxPoint[1] - globalMinPoint[1])/ itkImage->GetSpacing()[1]); // number of pixels along Y axis
  size[2] = static_cast<typename ItkImageType::SizeType::SizeValueType>((globalMaxPoint[2] - globalMinPoint[2])/ itkImage->GetSpacing()[2]); // number of pixels along Z axis
  regionOfInterest.SetSize(size);  
  regionOfInterest.Crop(itkImage->GetLargestPossibleRegion());  // fit region into source image 
  typename ItkImageType::IndexType pos = regionOfInterest.GetIndex();
  mitk::BoundingBox::PointType worldPos;
  worldPos[0] = pos[0]; worldPos[1] = pos[1]; worldPos[2] = pos[2];
  inputImageMitk->GetGeometry()->UnitsToMM(worldPos, worldPos);
  globalMinPoint[0] = worldPos[0]; globalMinPoint[1] = worldPos[1]; globalMinPoint[2] = worldPos[2]; 
  pos = regionOfInterest.GetIndex() + regionOfInterest.GetSize();
  worldPos[0] = pos[0]; worldPos[1] = pos[1]; worldPos[2] = pos[2];
  inputImageMitk->GetGeometry()->UnitsToMM(worldPos, worldPos);
  globalMaxPoint[0] = worldPos[0]; globalMaxPoint[1] = worldPos[1]; globalMaxPoint[2] = worldPos[2]; 
  
  /* Create output Image and output region */
  ItkRegionType outputRegion;
  typename ItkImageType::IndexType outputStart;
  outputStart[0] = 0;   // 3D Image. Output image starts at index (0, 0, 0)
  outputStart[1] = 0;
  outputStart[2] = 0;
  outputRegion.SetIndex( outputStart );
  outputRegion.SetSize( size );                         // output image has the same size as the input region  
  ItkImageType::Pointer itkOutputImage = ItkImageType::New();  // create new image
  itkOutputImage->SetRegions( outputRegion );           // set region data for the new image
  itkOutputImage->SetSpacing(itkImage->GetSpacing());   // copy spacing
  double outputOrigin[3] = {0, 0, 0};                   // Origin in local coordinates
  itkOutputImage->SetOrigin(outputOrigin);  
  itkOutputImage->Allocate();                           // allocate memory for pixel data
  /* define iterators for input and output images */
  ItkImageIteratorType inputIt(itkImage, regionOfInterest);
  ItkImageIteratorType outputIt(itkOutputImage, outputRegion);
  /* Cut the boundingbox out of the image by iterating through all pixels and checking if they are IsInside() */
  bool inside = false;
  m_OutsidePixelCount = 0;
  m_InsidePixelCount = 0;
  if (GetUseInsideValue()) // use a fixed value for each inside pixel (create a binary mask of the bounding object)
    for ( inputIt.GoToBegin(), outputIt.GoToBegin(); !inputIt.IsAtEnd(); ++inputIt, ++outputIt)
    {
      //itkImage->TransformIndexToPhysicalPoint(inputIt.GetIndex(), p);  // transform index of current pixel to world coordinate point
      p[0] = inputIt.GetIndex()[0]; p[1] = inputIt.GetIndex()[1]; p[2] = inputIt.GetIndex()[2]; 
      inputImageMitk->GetGeometry()->UnitsToMM(p, p);
      if(m_BoundingObject->IsInside(p))
      {
        outputIt.Value() = m_InsideValue;
        m_InsidePixelCount++;
      }
      else
      {
        outputIt.Value() = m_OutsideValue;
        m_OutsidePixelCount++;
      }
    }
  else // no fixed value for inside, use the pixel value of the original image (normal cutting)
    for ( inputIt.GoToBegin(), outputIt.GoToBegin(); !inputIt.IsAtEnd(); ++inputIt, ++outputIt)
    {
      //itkImage->TransformIndexToPhysicalPoint(inputIt.GetIndex(), p);  // transform index of current pixel to world coordinate point
      p[0] = inputIt.GetIndex()[0]; p[1] = inputIt.GetIndex()[1]; p[2] = inputIt.GetIndex()[2]; 
      inputImageMitk->GetGeometry()->UnitsToMM(p, p);
      if(m_BoundingObject->IsInside(p))
      {
        outputIt.Value() = inputIt.Value();
        m_InsidePixelCount++;
      }
      else
      {
        outputIt.Value() = m_OutsideValue;
        m_OutsidePixelCount++;
      }
    }  
  /* convert the itk image back to a mitk image and set it as output for this filter */
    CastToMitkImage(itkOutputImage, outputImage);
  /* Position the output Image to match the corresponding region of the input image */
  outputImage->GetGeometry()->GetVtkTransform()->Translate(globalMinPoint[0], globalMinPoint[1], globalMinPoint[2]);
  //outputImage->GetGeometry()->GetVtkTransform()->Translate(worldPos[0], worldPos[1], worldPos[2]);
}

} // of namespace mitk
#endif // of MITKBOUNDINGOBJECTCUTTER_TXX
