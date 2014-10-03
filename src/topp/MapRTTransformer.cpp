// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2014.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Clemens Groepl $
// $Authors: Marc Sturm, Clemens Groepl $
// --------------------------------------------------------------------------

#include <OpenMS/APPLICATIONS/MapAlignerBase.h>

using namespace OpenMS;
using namespace std;

//-------------------------------------------------------------
// Doxygen docu
//-------------------------------------------------------------

/**
    @page TOPP_MapRTTransformer MapRTTransformer

    @brief Applies retention time transformations to maps.

<CENTER>
    <table>
        <tr>
            <td ALIGN = "center" BGCOLOR="#EBEBEB"> potential predecessor tools </td>
            <td VALIGN="middle" ROWSPAN=2> \f$ \longrightarrow \f$ MapRTTransformer \f$ \longrightarrow \f$</td>
            <td ALIGN = "center" BGCOLOR="#EBEBEB"> potential successor tools </td>
        </tr>
        <tr>
            <td VALIGN="middle" ALIGN = "center" ROWSPAN=1> @ref TOPP_MapAlignerIdentification @n (or another alignment algorithm) </td>
            <td VALIGN="middle" ALIGN = "center" ROWSPAN=1> @ref TOPP_FeatureLinkerUnlabeled or @n @ref TOPP_FeatureLinkerUnlabeledQT </td>
        </tr>
    </table>
</CENTER>

    This tool can apply retention time transformations to different types of data (mzML, featureXML, consensusXML, and idXML files).
    The transformations might have been generated by a previous invocation of one of the MapAligner tools (linked below).
    However, the trafoXML file format is not very complicated, so it is relatively easy to write (or generate) your own files.
    Each input file will give rise to one output file.

    @see @ref TOPP_MapAlignerIdentification @ref TOPP_MapAlignerPoseClustering @ref TOPP_MapAlignerSpectrum

    With this tool it is also possible to invert transformations, or to fit a different model than originally specified to the retention time data in the transformation files. To fit a new model, choose a value other than "none" for the model type (see below).

    Since %OpenMS 1.8, the extraction of data for the alignment has been separate from the modeling of RT transformations based on that data. It is now possible to use different models independently of the chosen algorithm. The different available models are:
    - @ref OpenMS::TransformationModelLinear "linear": Linear model.
    - @ref OpenMS::TransformationModelBSpline "b_spline": Smoothing spline (non-linear).
    - @ref OpenMS::TransformationModelInterpolated "interpolated": Different types of interpolation.

    The following parameters control the modeling of RT transformations (they can be set in the "model" section of the INI file):
    @htmlinclude OpenMS_MapRTTransformerModel.parameters @n

    @note As output options, either 'out' or 'trafo_out' has to be provided. They can be used together.

    <B>The command line parameters of this tool are:</B> @n
    @verbinclude TOPP_MapRTTransformer.cli
    <B>INI file documentation of this tool:</B>
    @htmlinclude TOPP_MapRTTransformer.html

*/

// We do not want this class to show up in the docu:
/// @cond TOPPCLASSES

class TOPPMapRTTransformer :
  public TOPPMapAlignerBase
{

public:
  TOPPMapRTTransformer() :
    TOPPMapAlignerBase("MapRTTransformer", "Applies retention time transformations to maps.")
  {
  }

protected:
  void registerOptionsAndFlags_()
  {
    String file_formats = "mzML,featureXML,consensusXML,idXML";
    registerInputFileList_("in", "<files>", StringList(), "Input files to transform (separated by blanks)", false);
    setValidFormats_("in", ListUtils::create<String>(file_formats));
    registerOutputFileList_("out", "<files>", StringList(), "Output files separated by blanks. Either this option or 'trafo_out' have to be provided. They can be used together.", false);
    setValidFormats_("out", ListUtils::create<String>(file_formats));
    registerInputFileList_("trafo_in", "<files>", StringList(), "Transformations to apply (files separated by blanks)");
    setValidFormats_("trafo_in", ListUtils::create<String>("trafoXML"));
    registerOutputFileList_("trafo_out", "<files>", StringList(), "Transformation output files separated by blanks. Either this option or 'out' have to be provided. They can be used together.", false);
    setValidFormats_("trafo_out", ListUtils::create<String>("trafoXML"));
    registerFlag_("invert", "Invert transformations (approximatively) before applying them");
    addEmptyLine_();

    registerSubsection_("model", "Options to control the modeling of retention time transformations from data");
  }

  Param getSubsectionDefaults_(const String& /* section */) const
  {
    return getModelDefaults("none");
  }

  ExitCodes main_(int, const char**)
  {
    //-------------------------------------------------------------
    // parameter handling
    //-------------------------------------------------------------
    StringList ins = getStringList_("in");
    StringList outs = getStringList_("out");
    StringList trafo_ins = getStringList_("trafo_in");
    StringList trafo_outs = getStringList_("trafo_out");
    Param model_params = getParam_().copy("model:", true);
    String model_type = model_params.getValue("type");
    model_params = model_params.copy(model_type + ":", true);

    ProgressLogger progresslogger;
    progresslogger.setLogType(log_type_);

    //-------------------------------------------------------------
    // check for valid input
    //-------------------------------------------------------------
    // check whether numbers of input and transformation input files is equal:
    if (!ins.empty() && (ins.size() != trafo_ins.size()))
    {
      writeLog_("Error: The number of input and transformation input files has to be equal!");
      return ILLEGAL_PARAMETERS;
    }
    // check whether some kind of output file is given:
    if (outs.empty() && trafo_outs.empty())
    {
      writeLog_("Error: Either data output or transformation output files have to be provided!");
      return ILLEGAL_PARAMETERS;
    }
    // check whether number of input files equals number of output files:
    if (!outs.empty() && (ins.size() != outs.size()))
    {
      writeLog_("Error: The number of input and output files has to be equal!");
      return ILLEGAL_PARAMETERS;
    }
    if (!trafo_outs.empty() && (trafo_ins.size() != trafo_outs.size()))
    {
      writeLog_("Error: The number of transformation input and output files has to be equal!");
      return ILLEGAL_PARAMETERS;
    }

    //-------------------------------------------------------------
    // apply transformations
    //-------------------------------------------------------------
    progresslogger.startProgress(0, trafo_ins.size(),
                                 "applying RT transformations");
    for (Size i = 0; i < trafo_ins.size(); ++i)
    {
      TransformationXMLFile trafoxml;
      TransformationDescription trafo;
      trafoxml.load(trafo_ins[i], trafo);
      if (model_type != "none")
      {
        trafo.fitModel(model_type, model_params);
      }
      if (getFlag_("invert"))
      {
        trafo.invert();
      }
      if (!trafo_outs.empty())
      {
        trafoxml.store(trafo_outs[i], trafo);
      }
      if (!ins.empty()) // load input
      {
        String in_file = ins[i];
        FileTypes::Type in_type = FileHandler::getType(in_file);
        if (in_type == FileTypes::MZML)
        {
          MzMLFile file;
          MSExperiment<> map;
          file.load(in_file, map);
          MapAlignmentTransformer::transformSinglePeakMap(map, trafo);
          addDataProcessing_(map,
                             getProcessingInfo_(DataProcessing::ALIGNMENT));
          file.store(outs[i], map);
        }
        else if (in_type == FileTypes::FEATUREXML)
        {
          FeatureXMLFile file;
          FeatureMap<> map;
          file.load(in_file, map);
          MapAlignmentTransformer::transformSingleFeatureMap(map, trafo);
          addDataProcessing_(map,
                             getProcessingInfo_(DataProcessing::ALIGNMENT));
          file.store(outs[i], map);
        }
        else if (in_type == FileTypes::CONSENSUSXML)
        {
          ConsensusXMLFile file;
          ConsensusMap map;
          file.load(in_file, map);
          MapAlignmentTransformer::transformSingleConsensusMap(map, trafo);
          addDataProcessing_(map,
                             getProcessingInfo_(DataProcessing::ALIGNMENT));
          file.store(outs[i], map);
        }
        else if (in_type == FileTypes::IDXML)
        {
          IdXMLFile file;
          vector<ProteinIdentification> proteins;
          vector<PeptideIdentification> peptides;
          file.load(in_file, proteins, peptides);
          MapAlignmentTransformer::transformSinglePeptideIdentification(peptides,
                                                                        trafo);
          // no "data processing" section in idXML
          file.store(outs[i], proteins, peptides);
        }
      }
      progresslogger.setProgress(i);
    }
    progresslogger.endProgress();
    return EXECUTION_OK;
  }

};


int main(int argc, const char** argv)
{
  TOPPMapRTTransformer tool;
  return tool.main(argc, argv);
}

/// @endcond
