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
#ifndef __itkVectorGradientNDAnisotropicDiffusionFunction_h
#define __itkVectorGradientNDAnisotropicDiffusionFunction_h

#include "itkVectorAnisotropicDiffusionFunction.h"
#include "itkNeighborhoodAlgorithm.h"
#include "itkVectorNeighborhoodInnerProduct.h"
#include "itkDerivativeOperator.h"

namespace itk
{
/** \class VectorGradientNDAnisotropicDiffusionFunction
 *
 * This class is a simple extension of the
 * GradientNDAnisotropicDiffusionFunction to pixel types of multiple
 * components.  Vector components are diffused separately, but diffusion of
 * each component is limited by a conductance term which depends on all
 * components.
 *
 * For more information, please see GradientNDAnisotropicDiffusionFunction.
 *
 * \sa GradientNDAnisotropicDiffusionFunction
 * \sa VectorCurvatureNDAnisotropicDiffusionFunction
 * \sa AnisotropicDiffusionFunction
 * \ingroup ITK-AnisotropicSmoothing
 */
template< class TImage >
class ITK_EXPORT VectorGradientNDAnisotropicDiffusionFunction:
  public VectorAnisotropicDiffusionFunction< TImage >
{
public:
  /** Standard class typedefs. */
  typedef VectorGradientNDAnisotropicDiffusionFunction Self;
  typedef VectorAnisotropicDiffusionFunction< TImage > Superclass;
  typedef SmartPointer< Self >                         Pointer;
  typedef SmartPointer< const Self >                   ConstPointer;

  /** Method for creation through the object factory. */
  itkNewMacro(Self);

  /** Run-time type information (and related methods) */
  itkTypeMacro(VectorGradientNDAnisotropicDiffusionFunction,
               ScalarAnisotropicDiffusionFunction);

  /** Inherit some parameters from the superclass type. */
  typedef typename Superclass::ImageType        ImageType;
  typedef typename Superclass::PixelType        PixelType;
  typedef typename Superclass::TimeStepType     TimeStepType;
  typedef typename Superclass::RadiusType       RadiusType;
  typedef typename Superclass::NeighborhoodType NeighborhoodType;
  typedef typename Superclass::FloatOffsetType  FloatOffsetType;

  /** Extract vector and image dimension from superclass. */
  itkStaticConstMacro(ImageDimension, unsigned int,
                      Superclass::ImageDimension);
  itkStaticConstMacro(VectorDimension, unsigned int,
                      Superclass::VectorDimension);

  /** Type of a value in a vector (double, float, etc.) */
  typedef typename PixelType::ValueType ScalarValueType;

  /** Compute the equation value. */
  virtual PixelType ComputeUpdate(const NeighborhoodType & neighborhood,
                                  void *globalData,
                                  const FloatOffsetType & offset = FloatOffsetType(0.0)
                                  );

  /** This method is called prior to each iteration of the solver. */
  virtual void InitializeIteration()
  {
    m_K = this->GetAverageGradientMagnitudeSquared() * this->GetConductanceParameter()
          * this->GetConductanceParameter() * -2.0f;
  }

protected:
  VectorGradientNDAnisotropicDiffusionFunction();
  ~VectorGradientNDAnisotropicDiffusionFunction() {}
private:
  VectorGradientNDAnisotropicDiffusionFunction(const Self &); //purposely not
                                                              // implemented
  void operator=(const Self &);                               //purposely not

  // implemented

  /** Inner product function. */
  VectorNeighborhoodInnerProduct< ImageType > m_InnerProduct;

  /** Slices for the ND neighborhood. */
  std::slice x_slice[ImageDimension];
  std::slice xa_slice[ImageDimension][ImageDimension];
  std::slice xd_slice[ImageDimension][ImageDimension];

  /** Derivative operators. */
  DerivativeOperator< ScalarValueType,
                      itkGetStaticConstMacro(ImageDimension) > dx_op;

  /** Modified global average gradient magnitude term. */
  ScalarValueType m_K;

  static double m_MIN_NORM;

  SizeValueType m_Stride[ImageDimension];
  SizeValueType m_Center;
};
} // end namespace itk

#ifndef ITK_MANUAL_INSTANTIATION
#include "itkVectorGradientNDAnisotropicDiffusionFunction.txx"
#endif

#endif
