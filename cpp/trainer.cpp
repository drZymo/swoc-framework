#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <boost/filesystem.hpp>
#include <exception>
#include <iostream>
#include <algorithm>

#include <opencv2/ml/ml.hpp>

#include "ImageRepository.h"
#include "util.h"

using namespace std;
using namespace boost;
using namespace cv;

/**
 * Find visual word vocabulary by clustering all image patches
 */
cv::Mat findVocabulary(ImageClassList classes)
{
    cout << "(VOCABULARY) Loading..." << endl;
    PATCHES_MATRIX(patches);
    for (ImageClassList::iterator it = classes.begin(); it != classes.end(); it++)
    {
        it->loadPatches(patches);
    }

    // K-means to find visual words
    cout << "(VOCABULARY) Clustering..." << endl;
    int K = min(patches.size().height, VISUAL_WORD_COUNT);

    Mat finalKMeansLabels;
    Mat words;
    kmeans(patches, K, finalKMeansLabels, TermCriteria(CV_TERMCRIT_ITER|CV_TERMCRIT_EPS, 10000, 0.0001),
            5, // attempts
            KMEANS_PP_CENTERS, words);

    return words;
}

CvSVM trainClassifier(const cv::Mat &words, ImageClassList classes)
{
    // Now express every image as a histogram of visual words, retaining class labels
    cv::Mat trainingSet(0, words.size().height, CV_32F);
    cv::Mat trainingLabels(0, 1, CV_8U);

    cout << "( CLASSIF. ) Loading..." << endl;
    for (ImageClassList::iterator it = classes.begin(); it != classes.end(); it++)
    {
        it->loadVisualWords(trainingSet, trainingLabels, words);
    }

    // Train an SVM classifier based on the visual words vocabulary
    CvSVMParams params;
    params.svm_type    = CvSVM::C_SVC;
    params.kernel_type = CvSVM::RBF;
    params.gamma       = SVM_GAMMA;
    params.term_crit   = cvTermCriteria(CV_TERMCRIT_ITER, 100, 1e-6);

    // Train the SVM
    CvSVM svm;
    svm.train_auto(trainingSet, trainingLabels, Mat(), Mat(), params);
    return svm;
}

void validate(const ImageClassList &imageSet, const cv::Mat &words, const CvSVM &svm)
{
    cout << "Validating..." << endl;

    double accurate = 0;
    double total = 0;

    for (ImageClassList::const_iterator cls = imageSet.begin(); cls != imageSet.end(); cls++)
    {
        for (ImageList::const_iterator im = cls->images().begin(); im != cls->images().end(); im++)
        {
            cv::Mat imageWords(0, words.size().height, CV_32F);
            im->loadVisualWords(imageWords, words);

            float prediction = svm.predict(imageWords);

            total++;
            if (EQ(cls->intLabel(), prediction)) accurate++;
        }
    }

    cout.precision(3);
    cout << "Accuracy: " << (accurate / total) * 100 << "%" << endl;
}

int main(void) {
    try
    {
        // Initialize random seed
        std::srand (unsigned ( std::time(0) ) );

        // Load classes
        ImageRepository images("../images");
        ImageClassList classes = images.classes();
        
        // Split dataset into training and validation
        ImageClassList trainingSet;
        ImageClassList validationSet;
        partition(classes, TRAINING_SET_FRACTION, trainingSet, validationSet);

        // Train
        cv::Mat words = findVocabulary(trainingSet);  
        CvSVM   svm   = trainClassifier(words, trainingSet);

        // Save
        cv::FileStorage fs("learned-vocabulary.dat", FileStorage::WRITE);
        fs << "vocabulary" << words;
        fs.release();

        svm.save("learned-svm.dat");

        // Validation
        validate(validationSet, words, svm);

        return 0;
    }
    catch (const std::exception &ex)
    {
        std::cerr << ex.what() << endl;
    }
}
