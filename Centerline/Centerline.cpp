#include <gsl/gsl_errno.h>
#include <gsl/gsl_spline.h>

#include "CenterlineHelper.h"
#include "Centerline.h"
#include "../IOHelper.h"

using namespace DGtal;


/**
 * compare two Point by z value
 **/
struct PointOrderByZ {
      bool operator() (const Z3i::RealPoint &p1, const Z3i::RealPoint &p2) { return p1[2] < p2[2];}
} pointOrderByZ;


std::vector<Z3i::RealPoint>
Centerline::optimizeElasticForces(std::vector<Z3i::RealPoint> aFiberRaw, double epsilon=0.1){
    typedef typename Mesh<Z3i::RealPoint>::MeshFace Face;

    // For each fiber point we store the points associated to the section
    //std::map<unsigned int, std::vector<unsigned int> > mapPtMeshRing;
    std::vector<std::vector<unsigned int> > mapPtMeshRing;
    std::vector<double> radiusRing;
    std::vector<Z3i::RealPoint> optiMesh;

    std::vector<std::vector<unsigned int> > vectAssociations;

    double sumRadiis = 0.0;
    int nbFaces = 0;
    std::vector<Z3i::RealPoint> aFiber;
    for (unsigned int i = 0; i < aFiberRaw.size(); i++){
        Z3i::RealPoint ptFiber (aFiberRaw.at(i)[0], aFiberRaw.at(i)[1], aFiberRaw.at(i)[2]);
        //trace.error()<< "Dir: "<< dirImage(ptFiber)<<std::endl;
        std::vector<unsigned int> someFaces = CenterlineHelper::getSectionFacesFromDirection(mesh, aFiberRaw.at(i),
                dirImage(DGtal::PointVector<3, int>(ptFiber)), 0.1, 1.5*accRadius);//dirImage(ptFiber)

        if(someFaces.size() <= 0){
            continue;
        }
        aFiber.push_back(aFiberRaw.at(i));

        //trace.info()<<"nbfaces: "<<someFaces.size()<<std::endl;
        vectAssociations.push_back(someFaces);
        mapPtMeshRing.push_back(someFaces);

        double sumRadii = 0.0;
        for (unsigned int j = 0; j < someFaces.size(); j++){
            Face aFace = mesh.getFace(someFaces.at(j));
            //centroid
            Z3i::RealPoint ptMean (0,0,0);
            for (unsigned int k = 0; k < aFace.size(); k++){
                Z3i::RealPoint aPoint  = mesh.getVertex(aFace.at(k));
                ptMean += aPoint;
            }
            ptMean /= aFace.size();
            Z3i::RealPoint vecSM = ptMean - ptFiber;
            sumRadii += vecSM.norm();
            sumRadiis += vecSM.norm();
        }

        nbFaces += someFaces.size();
        radiusRing.push_back(sumRadii / someFaces.size());
        optiMesh.push_back(ptFiber);
    }
    //ring with no face
    double radii = sumRadiis / nbFaces;

    double deltaE;
    unsigned int  num = 0;
    double previousTot = 0;
    bool  first = true;
    trace.info() << "Starting optimisation with min precision diff :" << epsilon << "..."<< std::endl;
    while (first || deltaE > epsilon){
        num++;
        double totalError = 0.0;
        for (unsigned int i = 0; i < aFiber.size(); i++){
            //Z3i::RealPoint ptFiber (optiMesh.at(i)[0], optiMesh.at(i)[1], optiMesh.at(i)[2]);
            Z3i::RealPoint ptFiber = optiMesh.at(i);
            std::vector<unsigned int> someFaces = mapPtMeshRing[i];
            Z3i::RealPoint sumForces(0, 0, 0);
            unsigned int nb = 0;

            for (unsigned int j = 0; j < someFaces.size(); j++){
                Face aFace = mesh.getFace(someFaces.at(j));
                Z3i::RealPoint p0 = mesh.getVertex(aFace.at(0));
                Z3i::RealPoint p1 = mesh.getVertex(aFace.at(2));
                Z3i::RealPoint p2 = mesh.getVertex(aFace.at(1));
                Z3i::RealPoint vectorNormal = ((p1-p0).crossProduct(p2 - p0)).getNormalized();

                //centroid
                Z3i::RealPoint ptMean (0, 0, 0);
                for (unsigned int k = 0; k < aFace.size(); k++){
                    Z3i::RealPoint aPoint  = mesh.getVertex(aFace.at(k));
                    ptMean += aPoint;
                }
                ptMean /= aFace.size();
                Z3i::RealPoint vecSM = ptMean - ptFiber;

                double scala = vectorNormal.dot(vecSM)/vecSM.norm();
                double angle = acos(std::abs(scala));
                //Do not count face with vector normal too different to vector radial (vecSM)
                if(angle > M_PI/6){
                    continue;
                }

                double normPMoriente = vecSM.norm() - radii;
                Z3i::RealPoint vectPM = (vecSM/vecSM.norm())*normPMoriente;
                double error = normPMoriente*normPMoriente;
                totalError += error;
                sumForces += vectPM;
                nb++;
            }
            //project of sumForces to normal vector
            Z3i::RealPoint originalPointOnFib(aFiber.at(i)[0], aFiber.at(i)[1], aFiber.at(i)[2]);
            Z3i::RealPoint vectDir = dirImage(DGtal::PointVector<3, int>(originalPointOnFib));//dirImage(originalPointOnFib)
            Z3i::RealPoint sumForcesDir = vectDir.dot(sumForces)/vectDir.norm()/vectDir.norm()*vectDir;
            Z3i::RealPoint radialForces = sumForces - sumForcesDir;
            if(nb > 0){
                optiMesh[i] += radialForces/nb;
            }
        }

//        trace.info() << " Total error:" << totalError << std::endl;
        if (first) {
            deltaE = totalError;
            first = false;
        }else{
            //deltaE = std::abs(previousTot - totalError);
            deltaE = previousTot - totalError;
        }
        //std::cout << " Total error:" << totalError << std::endl;
        //std::cout << " delta E:" << deltaE << std::endl;
        previousTot = totalError;
    }

    return optiMesh;
}


bool
Centerline::isFurtherInside(const Z3i::RealPoint &aPoint,
        const Z3i::RealPoint &aPreviousPoint, double aDistance ){
    std::vector<Z3i::Point> aVectPoint;
    CenterlineHelper::getBallOrientedSurfaceSet(accImage, aVectPoint, aPoint,
            aPreviousPoint, aDistance, true, 1 );
    //trace.info() << "size vect front:" << aVectPoint.size();
    return aVectPoint.size()>0;
}


std::vector<Z3i::RealPoint>
Centerline::trackPatchCenter(const Z3i::Point &aStartingPoint, bool firstDirection=true){

    bool continueTracking = true;
    unsigned int patchImageSize= 2*accRadius;
    Z3i::RealPoint currentPoint = aStartingPoint;
    Z3i::RealPoint lastDirVect =  dirImage(aStartingPoint)/dirImage(aStartingPoint).norm();



    if (!firstDirection){
        lastDirVect *= -1.0;
    }
    //STRANGE !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    Z3i::RealPoint previousPoint = aStartingPoint-DGtal::PointVector<3, int>(lastDirVect*trackStep);

    std::vector<Z3i::RealPoint> trackingResult;
    int num = 0;

    Z3i::RealPoint lastDirVectToStartingPoint =  lastDirVect;

    while (continueTracking){
        trackingResult.push_back(currentPoint);
        Z3i::RealPoint dirVect = dirImage(DGtal::PointVector<3, int>(currentPoint))/dirImage(DGtal::PointVector<3, int>(currentPoint)).norm();//dirImage(currentPoint)/dirImage(currentPoint).norm()

        if(lastDirVect.dot(dirVect)<0){
            dirVect *= -1.0;
        }
        //dirVect = (dirVect + lastDirVect ).getNormalized();
        //trace.error() << "dirVect" << dirVect<<std::endl;
        continueTracking = isFurtherInside(currentPoint, previousPoint, trackStep );

        if (!continueTracking) {
            trace.info() << std::endl << "Dir  track "<< dirVect << std::endl;
            trace.info() << "Stopping front End of tube..." << std::endl;
            break;
        }
        previousPoint = currentPoint;

        Z3i::RealPoint pPatch = currentPoint + (dirVect*trackStep);

        if (!accImage.domain().isInside(DGtal::PointVector<3, int>(pPatch))){//!accImage.domain().isInside(pPatch)
            //trace.info() << "Not in domain patch :" <<pPatch<<  std::endl;
            //trace.info() << "current point :" <<currentPoint<<  std::endl;
            //trace.info() << "dur vect :" <<dirVect<<  std::endl;
            //trace.info() << "image val :" <<accImage(DGtal::PointVector<3, int>(currentPoint))<<  std::endl;//trace.info() << "image val :" <<accImage(currentPoint)<<  std::endl;
            continueTracking = false;
            break;
        }

        //std::cout <<"DIRVECT  : "<<dirVect<<std::endl;
        //std::cout <<"PATCHIMAGESIZE  : "<<patchImageSize<<std::endl;
        //std::cout <<"DEFAUT POINT  : "<<Z3i::Point(0,0,0)<<std::endl;

        //std::cout <<"ACCIMAGE DOMAIN  : "<<accImage.domain()<<std::endl;

        // Getting image patch from volume
        DGtal::functors::Point2DEmbedderIn3D<DGtal::Z3i::Domain >  embedder(accImage.domain(), DGtal::PointVector<3, int>(pPatch),dirVect, patchImageSize,  Z3i::Point(0,0,0));

        //DGtal::functors::Point2DEmbedderIn3D<DGtal::Z3i::Domain > embedder(accImage.domain(), DGtal::PointVector<3, int>(pPatch),dirVect, patchImageSize,  Z3i::Point(0,0,0));

        DGtal::Z2i::Domain domainImage2D (DGtal::Z2i::Point(0,0), DGtal::Z2i::Point(patchImageSize, patchImageSize));

        functors::Identity id;

        Image3D::Value valmax=0;
        /*for( Image3D::Domain::ConstIterator it = accImage.domain().begin(); it!= accImage.domain().end(); it++){
          Image3D::Value val = accImage(*it);
          if(val>valmax){
            valmax=val;
            std::cout <<"point  : "<<*it<<std::endl;
            std::cout <<"valmax : "<<valmax<<std::endl;
          }
        }
        exit(1);*/
        ImageAdapterExtractor patchImage(accImage, domainImage2D, embedder, id );
        ImageAdapterExtractor::Value valmax2=0;
                /*for( ImageAdapterExtractor::Domain::ConstIterator it = patchImage.domain().begin(); it!= patchImage.domain().end(); it++){
                  ImageAdapterExtractor::Value val = patchImage(*it);
                  if(val>valmax2){
                    valmax2=val;
                    std::cout <<"point : "<<*it<<std::endl;
                    std::cout <<"valmax : "<<valmax2<<std::endl;
                  }
                }*/

        Z2i::Point max2Dcoords = CenterlineHelper::getMaxCoords(patchImage);

        if(max2Dcoords[0]==0.0 && max2Dcoords[1]==0.0){
            continueTracking=false;
        }
        //lastDirVect = currentPoint - aStartingPoint;
        lastDirVect = dirVect;

        previousPoint = currentPoint;
        currentPoint = embedder(CenterlineHelper::getMaxCoords(patchImage));

        Z3i::RealPoint newDirVect = (currentPoint - previousPoint)/(currentPoint - previousPoint).norm();
        //trace.error()<<"angle"<<acos(newDirVect.dot(dirVect))<<std::endl;
        //should not go back
        if(acos(newDirVect.dot(dirVect)) < M_PI/2){
            //trace.error()<<"----------------" << std::endl;
            //trace.error()<<"currentPoint" << currentPoint<<std::endl;
            //trace.info()<<"previousPoint" << previousPoint<<std::endl;
            //trace.info()<<"dirVect" << dirVect<<std::endl;
            //trace.info()<<"newDirVect" << newDirVect<<std::endl;
            //trace.info()<<"angle"<<acos(newDirVect.dot(dirVect))<<std::endl;
            //trace.error()<<"detect..."<<std::endl;
            ////newDirVect *= -1;
            currentPoint = (pPatch + currentPoint)/2;
        }
        num++;
    }
    return trackingResult;
}


std::vector<Z3i::RealPoint>
Centerline::trackCenterline(const Z3i::Point &aStartingPoint){
    trace.info()<< "Track centerline..." << std::endl;
    std::vector<Z3i::RealPoint> skeletonResult;
    std::vector<Z3i::RealPoint> skeletonResultFront;

    skeletonResultFront = trackPatchCenter(aStartingPoint, true);

    trace.info()<< "End tracking front" << std::endl;

    if(skeletonResultFront.size()>0){
        for ( int i = skeletonResultFront.size()-1; i>0; i-- ){
            skeletonResult.push_back(skeletonResultFront.at(i));
        }
    }
    std::vector<Z3i::RealPoint> skeletonResultBack;
    skeletonResultBack = trackPatchCenter(aStartingPoint, false);

    trace.info()<< "End tracking back" << std::endl;
    for(unsigned int i= 0; i<skeletonResultBack.size(); i++){
        skeletonResult.push_back(skeletonResultBack.at(i));
    }

    std::sort(skeletonResult.begin(), skeletonResult.end(), pointOrderByZ);
    return skeletonResult;
}



Z3i::Point
Centerline::accumulate(double epsilonArea=0.1){
    trace.info()<<"Accumulate..."<<std::endl;
    unsigned int valMax = 0;
    ImageVector tmpImageVector(domain);

    Z3i::RealPoint pointPosMax;
    // all Face processing
    int nbF = 0;
    for (unsigned int iFace = 0; iFace< mesh.nbFaces(); iFace++){

        Face aFace = mesh.getFace(iFace);
        if(aFace.size()>4){
            trace.warning() << "ignoring face, not a triangular one: " << aFace.size() << " vertex" << std::endl;
            continue;
        }

        Z3i::RealPoint p0 = mesh.getVertex(aFace.at(0));
        Z3i::RealPoint p1 = mesh.getVertex(aFace.at(2));
        Z3i::RealPoint p2 = mesh.getVertex(aFace.at(1));

        Z3i::RealPoint scanDir = ((p1-p0).crossProduct(p2 - p0)).getNormalized();

        if (invertNormal){
            scanDir *= -1;
        }
        //trace.info() <<"SCANDIR :"<<scanDir << std::endl;
        Z3i::RealPoint centerPoint = (p0+p1+p2)/3.0;
        //trace.info() <<"CENTERPOINT :"<<centerPoint << std::endl;
        //test scan dir, should be removed, to verify?
        Z3i::RealPoint testPoint = centerPoint + scanDir*20;//OK|OK

        //DGtal::PointVector<3, int>(currentPoint)
        if(!accImage.domain().isInside(DGtal::PointVector<3, int>(testPoint))){
            scanDir *=-1;
            nbF++;
        }

        Z3i::RealPoint currentPoint = centerPoint;//OK
        Z3i::RealPoint previousPoint;
        //trace.info() <<"CURRENTPOINT :"<<currentPoint << std::endl;
        int c=0;
        while((currentPoint - centerPoint).norm() < accRadius){//OK

          c++;

            if(domain.isInside(DGtal::PointVector<3, int>(currentPoint)) && previousPoint != currentPoint){//  if(domain.isInside(currentPoint) && previousPoint != currentPoint){

                if(accImage(DGtal::PointVector<3, int>(currentPoint)) != 0){//if(accImage(currentPoint) != 0){

                    Z3i::RealPoint aVector = tmpImageVector(DGtal::PointVector<3, int>(currentPoint)).crossProduct(scanDir);//Z3i::RealPoint aVector = tmpImageVector(currentPoint).crossProduct(scanDir);

                    if(aVector.dot(dirImage(DGtal::PointVector<3, int>(currentPoint)))<0){
                        aVector *=-1;
                    }

                    if(aVector.norm() > epsilonArea){
                        dirImage.setValue(DGtal::PointVector<3, int>(currentPoint), (dirImage(DGtal::PointVector<3, int>(currentPoint))+aVector));//dirImage.setValue(currentPoint, (dirImage(currentPoint)+aVector));
                    }
                }
                tmpImageVector.setValue(DGtal::PointVector<3, int>(currentPoint), scanDir);//OK//tmpImageVector.setValue(currentPoint, scanDir);

                accImage.setValue(DGtal::PointVector<3, int>(currentPoint), accImage(DGtal::PointVector<3, int>(currentPoint))+1);


                previousPoint = currentPoint;//OK

                if( accImage(DGtal::PointVector<3, int>(currentPoint)) > valMax ) {

                    valMax = accImage(DGtal::PointVector<3, int>(currentPoint));//ok ok

                    pointPosMax = currentPoint;//not OK


                }
            }
            previousPoint = currentPoint;//OK

            currentPoint += scanDir;//OK

        }
    }
    //normalize
    for(ImageVector::Domain::ConstIterator it = dirImage.domain().begin(); it!= dirImage.domain().end(); it++){
        dirImage.setValue(*it, dirImage(*it)/dirImage(*it).norm());
    }

    //trace.info() << "POINT POS MAX : "<<pointPosMax<< std::endl;

    return DGtal::PointVector<3, int>(pointPosMax);


}


std::vector<Z3i::RealPoint>
Centerline::compute(){
    trace.info()<<"\tCompute centerline..."<<std::endl;
    Z3i::Point maxAccPoint = accumulate();
    //trace.info() << "POINT POS MAX : "<<maxAccPoint<< std::endl;
    std::vector<Z3i::RealPoint> vectFiber = trackCenterline(maxAccPoint);
    //trace.info() << "VECT FIBER : "<<vectFiber.at(vectFiber.size()-1)<< std::endl;
    std::vector<Z3i::RealPoint> optiFiber = optimizeElasticForces(vectFiber, 0.000001);
    //write centerline
    Mesh<Z3i::RealPoint> transMesh = mesh;
    for(unsigned int i =0; i< transMesh.nbFaces(); i++){
        transMesh.setFaceColor(i, DGtal::Color(120, 120 ,120, 180));
    }


    return optiFiber;
}
