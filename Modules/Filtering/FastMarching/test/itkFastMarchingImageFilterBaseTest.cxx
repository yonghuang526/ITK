/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/

#include "itkFastMarchingImageFilterBase.h"
#include "itkFastMarchingThresholdStoppingCriterion.h"
#include "itkTestingMacros.h"

template< unsigned int VDimension >
int FastMarchingImageFilterBase( )
{
  using PixelType = float;

  using ImageType = itk::Image< PixelType, VDimension >;
  typename ImageType::Pointer input = ImageType::New();

  using FastMarchingImageFilterType =
      itk::FastMarchingImageFilterBase< ImageType, ImageType >;
  typename FastMarchingImageFilterType::Pointer fastMarchingFilter = 
      FastMarchingImageFilterType::New();

  bool overrideOutputInformation = true;
  TEST_SET_GET_BOOLEAN( fastMarchingFilter, OverrideOutputInformation,
    overrideOutputInformation );

  FastMarchingImageFilterType::OutputSizeType outputSize;
  outputSize.Fill( 32 );
  fastMarchingFilter->SetOutputSize( outputSize );
  TEST_SET_GET_VALUE( outputSize, fastMarchingFilter->GetOutputSize() );

  FastMarchingImageFilterType::OutputRegionType outputRegion;
  outputRegion.SetSize( outputSize );
  fastMarchingFilter->SetOutputRegion( outputRegion );
  TEST_SET_GET_VALUE( outputRegion, fastMarchingFilter->GetOutputRegion() );

  FastMarchingImageFilterType::OutputSpacingType outputSpacing;
  outputSpacing.Fill( 1.0 );
  fastMarchingFilter->SetOutputSpacing( outputSpacing );
  TEST_SET_GET_VALUE( outputSpacing, fastMarchingFilter->GetOutputSpacing() );

  FastMarchingImageFilterType::OutputDirectionType outputDirection;
  outputDirection.SetIdentity();
  fastMarchingFilter->SetOutputDirection( outputDirection );
  TEST_SET_GET_VALUE( outputDirection, fastMarchingFilter->GetOutputDirection() );

  FastMarchingImageFilterType::OutputPointType outputOrigin;
  outputOrigin.Fill( 0.0 );
  fastMarchingFilter->SetOutputOrigin( outputOrigin );
  TEST_SET_GET_VALUE( outputOrigin, fastMarchingFilter->GetOutputOrigin() );


  fastMarchingFilter->SetInput( input );

  TRY_EXPECT_NO_EXCEPTION( fastMarchingFilter->Update() );

  typename ImageType::Pointer output = fastMarchingFilter->GetOutput();

  return EXIT_SUCCESS;
}


int itkFastMarchingImageFilterBaseTest( int , char * [] )
{
  // Exercise basic object methods
  // Done outside the helper function in the test because GCC is limited
  // when calling overloaded base class functions.
  const unsigned int Dimension = 2;
  typedef float                               PixelType;
  typedef itk::Image< PixelType, Dimension >  ImageType;

  typedef itk::FastMarchingImageFilterBase< ImageType, ImageType >
    FastMarchingImageFilterType;

  FastMarchingImageFilterType::Pointer fastMarchingFilter =
    FastMarchingImageFilterType::New();

  EXERCISE_BASIC_OBJECT_METHODS( fastMarchingFilter,
    FastMarchingImageFilterBase, FastMarchingBase );


  if( FastMarchingImageFilterBase< 2 >() == EXIT_FAILURE )
    {
    std::cerr << "Test failed!" << std::endl;
    return EXIT_FAILURE;
    }
  if( FastMarchingImageFilterBase< 3 >() == EXIT_FAILURE )
    {
    std::cerr << "Test failed!" << std::endl;
    return EXIT_FAILURE;
    }


  std::cout << "Test finished." <<std::endl;
  return EXIT_SUCCESS;
}
