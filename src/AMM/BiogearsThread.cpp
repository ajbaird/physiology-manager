#include "BiogearsThread.h"

using namespace biogears;

namespace AMM {
    class EventHandler : public SEEventHandler {
    public:
        // State flags
        bool irreversible = false;
        bool startOfExhale = false;
        bool startOfInhale = false;

        EventHandler(Logger *logger) : SEEventHandler() {}

        virtual void HandleAnesthesiaMachineEvent(CDM::enumAnesthesiaMachineEvent::value type, bool active,
                                                  const SEScalarTime *time = nullptr) {}

        virtual void
        HandlePatientEvent(CDM::enumPatientEvent::value type, bool active, const SEScalarTime *time = nullptr) {
            if (active) {
                switch (type) {
                    case CDM::enumPatientEvent::IrreversibleState:
                        LOG_INFO << " Patient has entered irreversible state";
                        irreversible = true;
                        break;
                    case CDM::enumPatientEvent::StartOfCardiacCycle:
                        break;
                    case CDM::enumPatientEvent::StartOfExhale:
                        startOfExhale = true;
                        startOfInhale = false;
                        break;
                    case CDM::enumPatientEvent::StartOfInhale:
                        startOfInhale = true;
                        startOfExhale = false;
                        break;
                    default:
                        LOG_INFO << " Patient has entered state : " << type;
                        break;
                }
            } else {
                switch (type) {
                    case CDM::enumPatientEvent::StartOfCardiacCycle:
                        break;
                    case CDM::enumPatientEvent::StartOfExhale:
                        startOfExhale = false;
                        break;
                    case CDM::enumPatientEvent::StartOfInhale:
                        startOfInhale = false;
                        break;
                    default:
                        LOG_INFO << " Patient has exited state : " << type;
                        break;
                }
            }
        }
    };

    std::vector <std::string> BiogearsThread::highFrequencyNodes;
    std::map<std::string, double (BiogearsThread::*)()> BiogearsThread::nodePathTable;

    BiogearsThread::BiogearsThread(const std::string &logFile) {
        try {
            m_pe = biogears::CreateBioGearsEngine("biogears.log");
        }
        catch (std::exception &e) {
            LOG_ERROR << "Error starting engine: " << e.what();
        }

        PopulateNodePathTable();

        running = false;
    }

    BiogearsThread::~BiogearsThread() {
        running = false;
        m_pe = nullptr;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void BiogearsThread::PopulateNodePathTable() {
        highFrequencyNodes.clear();
        nodePathTable.clear();

        // Legacy values
        nodePathTable["ECG"] = &BiogearsThread::GetECGWaveform;
        nodePathTable["HR"] = &BiogearsThread::GetHeartRate;
        nodePathTable["PATIENT_TIME"] = &BiogearsThread::GetPatientTime;
        nodePathTable["SIM_TIME"] = &BiogearsThread::GetSimulationTime;
        nodePathTable["LOGGING_STATUS"] = &BiogearsThread::GetLoggingStatus;

        nodePathTable["Patient_Age"] = &BiogearsThread::GetPatientAge;
        nodePathTable["Patient_Weight"] = &BiogearsThread::GetPatientWeight;
        nodePathTable["Patient_Gender"] = &BiogearsThread::GetPatientGender;
        nodePathTable["Patient_Height"] = &BiogearsThread::GetPatientHeight;
        nodePathTable["Patient_BodyFatFraction"] = &BiogearsThread::GetPatient_BodyFatFraction;


        nodePathTable["GCS_Value"] = &BiogearsThread::GetGCSValue;
        nodePathTable["IntracranialPressure"] = &BiogearsThread::GetIntracranialPressure;
        nodePathTable["CerebralPerfusionPressure"] = &BiogearsThread::GetCerebralPerfusionPressure;
        nodePathTable["CerebralBloodFlow"] = &BiogearsThread::GetCerebralBloodFlow;

        // Cardiovascular System
        nodePathTable["Cardiovascular_HeartRate"] = &BiogearsThread::GetHeartRate;
        nodePathTable["Cardiovascular_BloodVolume"] = &BiogearsThread::GetBloodVolume;
        nodePathTable["Cardiovascular_BloodLossPercentage"] = &BiogearsThread::GetBloodLossPercentage;
        nodePathTable["Cardiovascular_Arterial_Pressure"] = &BiogearsThread::GetArterialPressure;
        nodePathTable["Cardiovascular_Arterial_Mean_Pressure"] =
                &BiogearsThread::GetMeanArterialPressure;
        nodePathTable["Cardiovascular_Arterial_Systolic_Pressure"] =
                &BiogearsThread::GetArterialSystolicPressure;
        nodePathTable["Cardiovascular_Arterial_Diastolic_Pressure"] =
                &BiogearsThread::GetArterialDiastolicPressure;
        nodePathTable["Cardiovascular_CentralVenous_Mean_Pressure"] =
                &BiogearsThread::GetMeanCentralVenousPressure;
        nodePathTable["Cardiovascular_CardiacOutput"] = &BiogearsThread::GetCardiacOutput;

        //  Respiratory System
        nodePathTable["Respiratory_Respiration_Rate_RAW"] = &BiogearsThread::GetRawRespirationRate;
        nodePathTable["Respiratory_Respiration_Rate_MOD"] = &BiogearsThread::GetRespirationRate;
        nodePathTable["Respiratory_Respiration_Rate"] = &BiogearsThread::GetRawRespirationRate;
        nodePathTable["Respiratory_Inspiratory_Flow"] =
                &BiogearsThread::GetInspiratoryFlow;
        nodePathTable["Respiratory_TotalPressure"] = &BiogearsThread::GetRespiratoryTotalPressure;
        nodePathTable["Respiration_EndTidalCarbonDioxide"] =
                &BiogearsThread::GetEndTidalCarbonDioxidePressure;
        nodePathTable["Respiration_EndTidalCarbonDioxideFraction"] =
                &BiogearsThread::GetEndTidalCarbonDioxideFraction;
        nodePathTable["Respiratory_Tidal_Volume"] = &BiogearsThread::GetTidalVolume;
        nodePathTable["Respiratory_LungTotal_Volume"] = &BiogearsThread::GetTotalLungVolume;
        nodePathTable["Respiratory_LeftPleuralCavity_Volume"] =
                &BiogearsThread::GetLeftPleuralCavityVolume;
        nodePathTable["Respiratory_LeftLung_Volume"] = &BiogearsThread::GetLeftLungVolume;
        nodePathTable["Respiratory_LeftAlveoli_BaseCompliance"] =
                &BiogearsThread::GetLeftAlveoliBaselineCompliance;
        nodePathTable["Respiratory_RightPleuralCavity_Volume"] =
                &BiogearsThread::GetRightPleuralCavityVolume;
        nodePathTable["Respiratory_RightLung_Volume"] = &BiogearsThread::GetRightLungVolume;

        nodePathTable["Respiratory_LeftLung_Tidal_Volume"] = &BiogearsThread::GetLeftLungTidalVolume;
        nodePathTable["Respiratory_RightLung_Tidal_Volume"] =
                &BiogearsThread::GetRightLungTidalVolume;

        nodePathTable["Respiratory_PulmonaryResistance"] = &BiogearsThread::GetPulmonaryResistance;

        nodePathTable["Respiratory_RightAlveoli_BaseCompliance"] =
                &BiogearsThread::GetRightAlveoliBaselineCompliance;
        nodePathTable["Respiratory_CarbonDioxide_Exhaled"] = &BiogearsThread::GetExhaledCO2;

        // Energy system
        nodePathTable["Energy_Core_Temperature"] = &BiogearsThread::GetCoreTemperature;

        // Nervous
        nodePathTable["Nervous_GetPainVisualAnalogueScale"] =
                &BiogearsThread::GetPainVisualAnalogueScale;

        // Blood chemistry system
        nodePathTable["BloodChemistry_WhiteBloodCell_Count"] =
                &BiogearsThread::GetWhiteBloodCellCount;
        nodePathTable["BloodChemistry_RedBloodCell_Count"] = &BiogearsThread::GetRedBloodCellCount;
        nodePathTable["BloodChemistry_BloodUreaNitrogen_Concentration"] = &BiogearsThread::GetBUN;
        nodePathTable["BloodChemistry_Oxygen_Saturation"] = &BiogearsThread::GetOxygenSaturation;
        nodePathTable["BloodChemistry_CarbonMonoxide_Saturation"] = &BiogearsThread::GetCarbonMonoxideSaturation;
        nodePathTable["BloodChemistry_Hemaocrit"] = &BiogearsThread::GetHematocrit;
        nodePathTable["BloodChemistry_BloodPH_RAW"] = &BiogearsThread::GetRawBloodPH;
        nodePathTable["BloodChemistry_BloodPH_MOD"] = &BiogearsThread::GetModBloodPH;
        nodePathTable["BloodChemistry_BloodPH"] = &BiogearsThread::GetModBloodPH;
        nodePathTable["BloodChemistry_Arterial_CarbonDioxide_Pressure"] =
                &BiogearsThread::GetArterialCarbonDioxidePressure;
        nodePathTable["BloodChemistry_Arterial_Oxygen_Pressure"] =
                &BiogearsThread::GetArterialOxygenPressure;
        nodePathTable["BloodChemistry_VenousOxygenPressure"] =
                &BiogearsThread::GetVenousOxygenPressure;
        nodePathTable["BloodChemistry_VenousCarbonDioxidePressure"] =
                &BiogearsThread::GetVenousCarbonDioxidePressure;


        // Substances
        nodePathTable["Substance_Sodium"] = &BiogearsThread::GetSodium;
        nodePathTable["Substance_Sodium_Concentration"] = &BiogearsThread::GetSodiumConcentration;
        nodePathTable["Substance_Bicarbonate"] = &BiogearsThread::GetBicarbonate;
        nodePathTable["Substance_Bicarbonate_Concentration"] =
                &BiogearsThread::GetBicarbonateConcentration;
        nodePathTable["Substance_BaseExcess"] = &BiogearsThread::GetBaseExcess;
        nodePathTable["Substance_BaseExcess_RAW"] = &BiogearsThread::GetBaseExcessRaw;
        nodePathTable["Substance_Bicarbonate_RAW"] = &BiogearsThread::GetBicarbonateRaw;
        nodePathTable["Substance_Glucose_Concentration"] = &BiogearsThread::GetGlucoseConcentration;
        nodePathTable["Substance_Creatinine_Concentration"] =
                &BiogearsThread::GetCreatinineConcentration;

        nodePathTable["Substance_Hemoglobin_Concentration"] =
                &BiogearsThread::GetHemoglobinConcentration;
        nodePathTable["Substance_Oxyhemoglobin_Concentration"] =
                &BiogearsThread::GetOxyhemoglobinConcentration;
        nodePathTable["Substance_Carbaminohemoglobin_Concentration"] =
                &BiogearsThread::GetCarbaminohemoglobinConcentration;
        nodePathTable["Substance_OxyCarbaminohemoglobin_Concentration"] =
                &BiogearsThread::GetOxyCarbaminohemoglobinConcentration;
        nodePathTable["Substance_Carboxyhemoglobin_Concentration"] =
                &BiogearsThread::GetCarboxyhemoglobinConcentration;

        nodePathTable["Anion_Gap"] = &BiogearsThread::GetAnionGap;

        nodePathTable["Substance_Ionized_Calcium"] = &BiogearsThread::GetIonizedCalcium;
        nodePathTable["Substance_Calcium_Concentration"] = &BiogearsThread::GetCalciumConcentration;
        nodePathTable["Substance_Albumin_Concentration"] = &BiogearsThread::GetAlbuminConcentration;

        nodePathTable["Substance_Lactate_Concentration"] = &BiogearsThread::GetLactateConcentration;
        nodePathTable["Substance_Lactate_Concentration_mmol"] =
                &BiogearsThread::GetLactateConcentrationMMOL;

        nodePathTable["MetabolicPanel_Bilirubin"] = &BiogearsThread::GetTotalBilirubin;
        nodePathTable["MetabolicPanel_Protein"] = &BiogearsThread::GetTotalProtein;
        nodePathTable["MetabolicPanel_CarbonDioxide"] = &BiogearsThread::GetCO2;
        nodePathTable["MetabolicPanel_Potassium"] = &BiogearsThread::GetPotassium;
        nodePathTable["MetabolicPanel_Chloride"] = &BiogearsThread::GetChloride;

        nodePathTable["CompleteBloodCount_Platelet"] = &BiogearsThread::GetPlateletCount;

        nodePathTable["Renal_UrineProductionRate"] = &BiogearsThread::GetUrineProductionRate;
        nodePathTable["Urinalysis_SpecificGravity"] = &BiogearsThread::GetUrineSpecificGravity;
        nodePathTable["Renal_UrineOsmolality"] = &BiogearsThread::GetUrineOsmolality;
        nodePathTable["Renal_UrineOsmolarity"] = &BiogearsThread::GetUrineOsmolarity;
        nodePathTable["Renal_BladderGlucose"] = &BiogearsThread::GetBladderGlucose;

        nodePathTable["ShuntFraction"] = &BiogearsThread::GetShuntFraction;

        // Label which nodes are high-frequency
        highFrequencyNodes = {"ECG",
                              "Cardiovascular_HeartRate",
                              "Respiratory_TotalPressure",
                              "Respiratory_Inspiratory_Flow",
                              "Cardiovascular_Arterial_Pressure",
                              "Respiratory_CarbonDioxide_Exhaled",
                              "Respiratory_LungTotal_Volume",
                              "Respiratory_Respiration_Rate"};
    }

    double BiogearsThread::GetLoggingStatus() {
        if (logging_enabled) {
            return double(1);
        } else {
            return double(0);
        }
    }

    std::map<std::string, double (BiogearsThread::*)()> *BiogearsThread::GetNodePathTable() {
        return &nodePathTable;
    }

    void BiogearsThread::Shutdown() {

    }

    void BiogearsThread::StartSimulation() {
        if (!running) {
            running = true;
        }
    }

    void BiogearsThread::StopSimulation() {
        if (running) {
            running = false;
        }
    }

    bool BiogearsThread::LoadPatient(const std::string &patientFile) {
        if (m_pe == nullptr) {
            LOG_ERROR << "Unable to load state, Biogears has not been initialized.";
            return false;
        }

        LOG_INFO << "Loading patient file " << patientFile;
        m_mutex.lock();
        try {
            if (!m_pe->InitializeEngine(patientFile)) {
                LOG_ERROR << "Error loading patient";
                m_mutex.unlock();
                return false;
            }
        }
        catch (std::exception &e) {
            LOG_ERROR << "Exception loading patient: " << e.what();
            m_mutex.unlock();
            return false;
        }
        m_mutex.unlock();

        LOG_DEBUG << "Preloading substances";
        // preload substances
        m_mutex.lock();
        sodium = m_pe->GetSubstanceManager().GetSubstance("Sodium");
        glucose = m_pe->GetSubstanceManager().GetSubstance("Glucose");
        creatinine = m_pe->GetSubstanceManager().GetSubstance("Creatinine");
        calcium = m_pe->GetSubstanceManager().GetSubstance("Calcium");
        bicarbonate = m_pe->GetSubstanceManager().GetSubstance("Bicarbonate");
        albumin = m_pe->GetSubstanceManager().GetSubstance("Albumin");
        CO2 = m_pe->GetSubstanceManager().GetSubstance("CarbonDioxide");
        N2 = m_pe->GetSubstanceManager().GetSubstance("Nitrogen");
        O2 = m_pe->GetSubstanceManager().GetSubstance("Oxygen");
        CO = m_pe->GetSubstanceManager().GetSubstance("CarbonMonoxide");
        Hb = m_pe->GetSubstanceManager().GetSubstance("Hemoglobin");
        HbO2 = m_pe->GetSubstanceManager().GetSubstance("Oxyhemoglobin");
        HbCO2 = m_pe->GetSubstanceManager().GetSubstance("Carbaminohemoglobin");
        HbCO = m_pe->GetSubstanceManager().GetSubstance("Carboxyhemoglobin");
        HbO2CO2 = m_pe->GetSubstanceManager().GetSubstance("OxyCarbaminohemoglobin");

        potassium = m_pe->GetSubstanceManager().GetSubstance("Potassium");
        chloride = m_pe->GetSubstanceManager().GetSubstance("Chloride");
        lactate = m_pe->GetSubstanceManager().GetSubstance("Lactate");

        // preload compartments
        carina = m_pe->GetCompartments().GetGasCompartment(BGE::PulmonaryCompartment::Trachea);
        leftLung = m_pe->GetCompartments().GetGasCompartment(BGE::PulmonaryCompartment::LeftLung);
        rightLung = m_pe->GetCompartments().GetGasCompartment(BGE::PulmonaryCompartment::RightLung);
        bladder = m_pe->GetCompartments().GetLiquidCompartment(BGE::UrineCompartment::Bladder);

        // m_patient = m_pe->GetPatient();

        m_mutex.unlock();

        startingBloodVolume = 5400.00;
        currentBloodVolume = startingBloodVolume;

        if (logging_enabled) {
            std::string logFilename = Utility::getTimestampedFilename("./logs/AMM_Output_", ".csv");
            LOG_INFO << "Initializing log file: " << logFilename;

            std::fstream fs;
            fs.open(logFilename, std::ios::out);
            fs.close();

            m_pe->GetEngineTrack()->GetDataRequestManager().Clear();
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "HeartRate", biogears::FrequencyUnit::Per_min);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "MeanArterialPressure", biogears::PressureUnit::mmHg);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "SystolicArterialPressure", biogears::PressureUnit::mmHg);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "DiastolicArterialPressure", biogears::PressureUnit::mmHg);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "RespirationRate", biogears::FrequencyUnit::Per_min);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "TidalVolume", biogears::VolumeUnit::mL);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "TotalLungVolume", biogears::VolumeUnit::mL);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                    BGE::PulmonaryCompartment::LeftLung, "Volume");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                    BGE::PulmonaryCompartment::RightLung, "Volume");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "OxygenSaturation");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                    BGE::PulmonaryCompartment::Trachea, "InFlow");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "BloodVolume", biogears::VolumeUnit::mL);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "ArterialBloodPH");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateSubstanceDataRequest().Set(
                    *lactate, "BloodConcentration", biogears::MassPerVolumeUnit::ug_Per_mL);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "UrineProductionRate", biogears::VolumePerTimeUnit::mL_Per_min);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                    BGE::PulmonaryCompartment::Trachea, *O2, "PartialPressure");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                    BGE::PulmonaryCompartment::Trachea, *CO2, "PartialPressure");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                    BGE::PulmonaryCompartment::Trachea, "Pressure", biogears::PressureUnit::cmH2O);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "BaseExcess", biogears::AmountPerVolumeUnit::mmol_Per_L);
            m_pe->GetEngineTrack()->GetDataRequestManager().SetResultsFilename(logFilename);
        }

        try {
            LOG_DEBUG << "Attaching event handler";
            myEventHandler = new EventHandler(m_pe->GetLogger());
            m_pe->SetEventHandler(myEventHandler);
        } catch (std::exception &e) {
            LOG_ERROR << "Error attaching event handler: " << e.what();
        }

        return true;
    }

    bool BiogearsThread::LoadState(const std::string &stateFile, double sec) {
        if (m_pe == nullptr) {
            LOG_ERROR << "Unable to load state, Biogears has not been initialized.";
            return false;
        }

        auto *startTime = new biogears::SEScalarTime();
        startTime->SetValue(sec, biogears::TimeUnit::s);

        LOG_INFO << "Loading state file " << stateFile << " at position " << sec << " seconds";
        m_mutex.lock();
        try {

            if (!m_pe->LoadState(stateFile, startTime)) {
                LOG_ERROR << "Error loading state";
                m_mutex.unlock();
                return false;
            }
        }
        catch (std::exception &e) {
            LOG_ERROR << "Exception loading state: " << e.what();
            m_mutex.unlock();
            return false;
        }
        m_mutex.unlock();

        LOG_DEBUG << "Preloading substances";
        // preload substances
        m_mutex.lock();
        sodium = m_pe->GetSubstanceManager().GetSubstance("Sodium");
        glucose = m_pe->GetSubstanceManager().GetSubstance("Glucose");
        creatinine = m_pe->GetSubstanceManager().GetSubstance("Creatinine");
        calcium = m_pe->GetSubstanceManager().GetSubstance("Calcium");
        bicarbonate = m_pe->GetSubstanceManager().GetSubstance("Bicarbonate");
        albumin = m_pe->GetSubstanceManager().GetSubstance("Albumin");
        CO2 = m_pe->GetSubstanceManager().GetSubstance("CarbonDioxide");
        N2 = m_pe->GetSubstanceManager().GetSubstance("Nitrogen");
        O2 = m_pe->GetSubstanceManager().GetSubstance("Oxygen");
        CO = m_pe->GetSubstanceManager().GetSubstance("CarbonMonoxide");
        Hb = m_pe->GetSubstanceManager().GetSubstance("Hemoglobin");
        HbO2 = m_pe->GetSubstanceManager().GetSubstance("Oxyhemoglobin");
        HbCO2 = m_pe->GetSubstanceManager().GetSubstance("Carbaminohemoglobin");
        HbCO = m_pe->GetSubstanceManager().GetSubstance("Carboxyhemoglobin");
        HbO2CO2 = m_pe->GetSubstanceManager().GetSubstance("OxyCarbaminohemoglobin");

        potassium = m_pe->GetSubstanceManager().GetSubstance("Potassium");
        chloride = m_pe->GetSubstanceManager().GetSubstance("Chloride");
        lactate = m_pe->GetSubstanceManager().GetSubstance("Lactate");

        // preload compartments
        carina = m_pe->GetCompartments().GetGasCompartment(BGE::PulmonaryCompartment::Trachea);
        leftLung = m_pe->GetCompartments().GetGasCompartment(BGE::PulmonaryCompartment::LeftLung);
        rightLung = m_pe->GetCompartments().GetGasCompartment(BGE::PulmonaryCompartment::RightLung);
        bladder = m_pe->GetCompartments().GetLiquidCompartment(BGE::UrineCompartment::Bladder);
        m_mutex.unlock();

        startingBloodVolume = 5400.00;
        currentBloodVolume = startingBloodVolume;

        if (logging_enabled) {
            std::string logFilename = Utility::getTimestampedFilename("./logs/AMM_Output_", ".csv");
            LOG_INFO << "Initializing log file: " << logFilename;

            std::fstream fs;
            fs.open(logFilename, std::ios::out);
            fs.close();

            m_pe->GetEngineTrack()->GetDataRequestManager().Clear();
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "HeartRate", biogears::FrequencyUnit::Per_min);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "MeanArterialPressure", biogears::PressureUnit::mmHg);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "SystolicArterialPressure", biogears::PressureUnit::mmHg);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "DiastolicArterialPressure", biogears::PressureUnit::mmHg);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "RespirationRate", biogears::FrequencyUnit::Per_min);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "TidalVolume", biogears::VolumeUnit::mL);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "TotalLungVolume", biogears::VolumeUnit::mL);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                    BGE::PulmonaryCompartment::LeftLung, "Volume");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                    BGE::PulmonaryCompartment::RightLung, "Volume");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "OxygenSaturation");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                    BGE::PulmonaryCompartment::Trachea, "InFlow");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "BloodVolume", biogears::VolumeUnit::mL);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "ArterialBloodPH");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateSubstanceDataRequest().Set(
                    *lactate, "BloodConcentration", biogears::MassPerVolumeUnit::ug_Per_mL);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "UrineProductionRate", biogears::VolumePerTimeUnit::mL_Per_min);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                    BGE::PulmonaryCompartment::Trachea, *O2, "PartialPressure");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                    BGE::PulmonaryCompartment::Trachea, *CO2, "PartialPressure");
            m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                    BGE::PulmonaryCompartment::Trachea, "Pressure", biogears::PressureUnit::cmH2O);
            m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                    "BaseExcess", biogears::AmountPerVolumeUnit::mmol_Per_L);
            m_pe->GetEngineTrack()->GetDataRequestManager().SetResultsFilename(logFilename);
        }

        try {
            LOG_DEBUG << "Attaching event handler";
            myEventHandler = new EventHandler(m_pe->GetLogger());
            m_pe->SetEventHandler(myEventHandler);
        } catch (std::exception &e) {
            LOG_ERROR << "Error attaching event handler: " << e.what();
        }

        return true;
    }

    bool BiogearsThread::SaveState(const std::string &stateFile) {
        if (m_pe == nullptr) {
            LOG_ERROR << "Unable to save state, Biogears has not been initialized.";
            return false;
        }
        m_mutex.lock();
        m_pe->SaveStateToFile(stateFile);
        // m_pe->SaveState(stateFile);
        m_mutex.unlock();
        return true;
    }

    bool BiogearsThread::Execute(std::function<std::unique_ptr<biogears::PhysiologyEngine>(
            std::unique_ptr < biogears::PhysiologyEngine > && )>
                                 func) {
        m_pe = func(std::move(m_pe));
        return true;
    }

    void BiogearsThread::SetLastFrame(int lF) {
        lastFrame = lF;
    }

    void BiogearsThread::SetLogging(bool log) {
        logging_enabled = log;
    }

    bool BiogearsThread::ExecuteXMLCommand(const std::string &cmd) {
        char *tmpname = strdup("/tmp/tmp_amm_xml_XXXXXX");
        std::ofstream out(tmpname);
        if (out.is_open()) {
            out << cmd;

            out.close();
            if (!LoadScenarioFile(tmpname)) {
                LOG_ERROR << "Unable to load scenario file from temp.";
                return false;
            } else {
                return true;
            }
        } else {
            LOG_ERROR << "Unable to open file.";
        }

        return true;
    }

    bool file_exists(const char *fileName) {
        std::ifstream infile(fileName);
        return infile.good();
    }

// Load a scenario from an XML file, apply conditions and iterate through the actions
// This bypasses the standard BioGears ExecuteScenario method to avoid resetting the BioGears
// engine
    bool BiogearsThread::LoadScenarioFile(const std::string &scenarioFile) {
        if (m_pe == nullptr) {
            LOG_ERROR << "Unable to load scenario, Biogears has not been initialized.";
            return false;
        }

        if (!file_exists(scenarioFile.c_str())) {
            LOG_WARNING << "Scenario/action file does not exist: " << scenarioFile;
            return false;
        }


        biogears::SEScenario sce(m_pe->GetSubstanceManager());
        sce.Load(scenarioFile);

        if (scenarioLoading) {
            if (sce.HasEngineStateFile()) {
                if (!m_pe->LoadState(sce.GetEngineStateFile())) {
                    LOG_ERROR << "Unable to load state file.";
                    return false;
                }
            } else if (sce.HasInitialParameters()) {
                SEScenarioInitialParameters &sip = sce.GetInitialParameters();
                if (sip.HasPatientFile()) {
                    std::vector<const SECondition *> conditions;
                    for (SECondition *c : sip.GetConditions())
                        conditions.push_back(c);// Copy to const
                    if (!m_pe->InitializeEngine(sip.GetPatientFile(), &conditions, &sip.GetConfiguration())) {

                        LOG_ERROR << "Unable to load patient file.";
                        return false;
                    }
                } else if (sip.HasPatient()) {
                    std::vector<const SECondition *> conditions;
                    for (SECondition *c : sip.GetConditions())
                        conditions.push_back(c);// Copy to const
                    if (!m_pe->InitializeEngine(sip.GetPatient(), &conditions, &sip.GetConfiguration())) {
                        LOG_ERROR << "Unable to load conditions.";
                        return false;
                    }
                }
            }

            LOG_DEBUG << "Preloading substances";
            // preload substances
            m_mutex.lock();
            sodium = m_pe->GetSubstanceManager().GetSubstance("Sodium");
            glucose = m_pe->GetSubstanceManager().GetSubstance("Glucose");
            creatinine = m_pe->GetSubstanceManager().GetSubstance("Creatinine");
            calcium = m_pe->GetSubstanceManager().GetSubstance("Calcium");
            bicarbonate = m_pe->GetSubstanceManager().GetSubstance("Bicarbonate");
            albumin = m_pe->GetSubstanceManager().GetSubstance("Albumin");
            CO2 = m_pe->GetSubstanceManager().GetSubstance("CarbonDioxide");
            N2 = m_pe->GetSubstanceManager().GetSubstance("Nitrogen");
            O2 = m_pe->GetSubstanceManager().GetSubstance("Oxygen");
            CO = m_pe->GetSubstanceManager().GetSubstance("CarbonMonoxide");
            Hb = m_pe->GetSubstanceManager().GetSubstance("Hemoglobin");
            HbO2 = m_pe->GetSubstanceManager().GetSubstance("Oxyhemoglobin");
            HbCO2 = m_pe->GetSubstanceManager().GetSubstance("Carbaminohemoglobin");
            HbCO = m_pe->GetSubstanceManager().GetSubstance("Carboxyhemoglobin");
            HbO2CO2 = m_pe->GetSubstanceManager().GetSubstance("OxyCarbaminohemoglobin");

            potassium = m_pe->GetSubstanceManager().GetSubstance("Potassium");
            chloride = m_pe->GetSubstanceManager().GetSubstance("Chloride");
            lactate = m_pe->GetSubstanceManager().GetSubstance("Lactate");

            // preload compartments
            carina = m_pe->GetCompartments().GetGasCompartment(BGE::PulmonaryCompartment::Trachea);
            leftLung = m_pe->GetCompartments().GetGasCompartment(BGE::PulmonaryCompartment::LeftLung);
            rightLung = m_pe->GetCompartments().GetGasCompartment(BGE::PulmonaryCompartment::RightLung);
            bladder = m_pe->GetCompartments().GetLiquidCompartment(BGE::UrineCompartment::Bladder);
            m_mutex.unlock();

            startingBloodVolume = 5400.00;
            currentBloodVolume = startingBloodVolume;

            if (logging_enabled) {
                std::string logFilename = Utility::getTimestampedFilename("./logs/AMM_Output_", ".csv");
                LOG_INFO << "Initializing log file: " << logFilename;

                std::fstream fs;
                fs.open(logFilename, std::ios::out);
                fs.close();

                m_pe->GetEngineTrack()->GetDataRequestManager().Clear();
                m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                        "HeartRate", biogears::FrequencyUnit::Per_min);
                m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                        "MeanArterialPressure", biogears::PressureUnit::mmHg);
                m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                        "SystolicArterialPressure", biogears::PressureUnit::mmHg);
                m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                        "DiastolicArterialPressure", biogears::PressureUnit::mmHg);
                m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                        "RespirationRate", biogears::FrequencyUnit::Per_min);
                m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                        "TidalVolume", biogears::VolumeUnit::mL);
                m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                        "TotalLungVolume", biogears::VolumeUnit::mL);
                m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                        BGE::PulmonaryCompartment::LeftLung, "Volume");
                m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                        BGE::PulmonaryCompartment::RightLung, "Volume");
                m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                        "OxygenSaturation");
                m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                        BGE::PulmonaryCompartment::Trachea, "InFlow");
                m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                        "BloodVolume", biogears::VolumeUnit::mL);
                m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                        "ArterialBloodPH");
                m_pe->GetEngineTrack()->GetDataRequestManager().CreateSubstanceDataRequest().Set(
                        *lactate, "BloodConcentration", biogears::MassPerVolumeUnit::ug_Per_mL);
                m_pe->GetEngineTrack()->GetDataRequestManager().CreatePhysiologyDataRequest().Set(
                        "UrineProductionRate", biogears::VolumePerTimeUnit::mL_Per_min);
                m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                        BGE::PulmonaryCompartment::Trachea, *O2, "PartialPressure");
                m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                        BGE::PulmonaryCompartment::Trachea, *CO2, "PartialPressure");
                m_pe->GetEngineTrack()->GetDataRequestManager().CreateGasCompartmentDataRequest().Set(
                        BGE::PulmonaryCompartment::Trachea, "Pressure", biogears::PressureUnit::cmH2O);
                m_pe->GetEngineTrack()->GetDataRequestManager().SetResultsFilename(logFilename);
            }

            try {
                LOG_DEBUG << "Attaching event handler";
                myEventHandler = new EventHandler(m_pe->GetLogger());
                m_pe->SetEventHandler(myEventHandler);
            } catch (std::exception &e) {
                LOG_ERROR << "Error attaching event handler: " << e.what();
            }
            scenarioLoading = false;
        }

        LOG_INFO << "Executing actions";
        SEAdvanceTime *adv;
        // Now run the scenario actions
        for (SEAction *a : sce.GetActions()) {
            adv = dynamic_cast<SEAdvanceTime *>(a);
            if (adv != nullptr) {
                m_mutex.lock();
                try {
                    auto begin = std::chrono::high_resolution_clock::now();
                    LOG_INFO << "Simulating " << adv->GetTime(TimeUnit::s) << " seconds...";
                    m_pe->AdvanceModelTime(adv->GetTime(TimeUnit::s), TimeUnit::s);
                    auto end = std::chrono::high_resolution_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
                    LOG_INFO << "Done simulating time advancement. Simulated " << adv->GetTime(TimeUnit::s) << "s in "
                             << elapsed.count() * 1e-9 << "s.";
                }
                catch (std::exception &e) {
                    LOG_ERROR << "Error advancing time: " << e.what();
                }
                m_mutex.unlock();
            } else {
                m_mutex.lock();
                try {
                    if (!m_pe->ProcessAction(*a)) {
                        LOG_ERROR << "Unable to process action.";
                    }
                }
                catch (std::exception &e) {
                    LOG_ERROR << "Error processing action: " << e.what();
                }
                m_mutex.unlock();
            }
        }
        LOG_INFO << "Done loading scenario and executing actions.";
        return true;
    }

    void BiogearsThread::AdvanceTimeTick() {
        if (m_pe == nullptr) {
            LOG_ERROR << "Unable to advance time, Biogears has not been initialized.";
            return;
        }

        if (!running) {
            LOG_ERROR << "Cannot advance time, simulation is not running.";
            return;
        }

        if (myEventHandler != nullptr) {
            if (myEventHandler->irreversible && !irreversible) {
                irreversible = true;
            }

            startOfInhale = myEventHandler->startOfInhale;
            startOfExhale = myEventHandler->startOfExhale;
        }

        m_mutex.lock();
        try {
            m_pe->AdvanceModelTime();
            if (logging_enabled) {
                if ((lastFrame % loggingFrequency) == 0) {
                    m_pe->GetEngineTrack()->TrackData(m_pe->GetSimulationTime(biogears::TimeUnit::s));
                }
            }
        }
        catch (std::exception &e) {
            LOG_ERROR << "Error advancing time: " << e.what();
        }
        m_mutex.unlock();
    }

    double BiogearsThread::GetShutdownMessage() {
        return -1;
    }

    double BiogearsThread::GetSimulationTime() {
        return lastFrame / 50;
    }

    double BiogearsThread::GetPatientTime() {
        return 0.0;
        // return m_pe->GetSimulationTime(biogears::TimeUnit::s);
    }

    double BiogearsThread::GetNodePath(const std::string &nodePath) {
        std::map<std::string, double (BiogearsThread::*)()>::iterator entry;
        entry = nodePathTable.find(nodePath);
        if (entry != nodePathTable.end()) {
            return (this->*(entry->second))();
        }

        LOG_ERROR << "Unable to access nodepath " << nodePath;
        return 0;
    }

    double BiogearsThread::GetBloodVolume() {
        currentBloodVolume = m_pe->GetCardiovascularSystem()->GetBloodVolume(biogears::VolumeUnit::mL);
        return currentBloodVolume;
    }

    double BiogearsThread::GetBloodLossPercentage() {
        double loss = (startingBloodVolume - currentBloodVolume) / startingBloodVolume;
        return loss;
    }

    double BiogearsThread::GetHeartRate() {
        return m_pe->GetCardiovascularSystem()->GetHeartRate(biogears::FrequencyUnit::Per_min);
    }

// SYS (ART) - Arterial Systolic Pressure - mmHg
    double BiogearsThread::GetArterialSystolicPressure() {
        return m_pe->GetCardiovascularSystem()->GetSystolicArterialPressure(
                biogears::PressureUnit::mmHg);
    }

// DIA (ART) - Arterial Diastolic Pressure - mmHg
    double BiogearsThread::GetArterialDiastolicPressure() {
        return m_pe->GetCardiovascularSystem()->GetDiastolicArterialPressure(
                biogears::PressureUnit::mmHg);
    }

// MAP (ART) - Mean Arterial Pressure - mmHg
    double BiogearsThread::GetMeanArterialPressure() {
        return m_pe->GetCardiovascularSystem()->GetMeanArterialPressure(biogears::PressureUnit::mmHg);
    }

// AP - Arterial Pressure - mmHg
    double BiogearsThread::GetArterialPressure() {
        return m_pe->GetCardiovascularSystem()->GetArterialPressure(biogears::PressureUnit::mmHg);
    }

// CVP - Central Venous Pressure - mmHg
    double BiogearsThread::GetMeanCentralVenousPressure() {
        return m_pe->GetCardiovascularSystem()->GetMeanCentralVenousPressure(
                biogears::PressureUnit::mmHg);
    }

// MCO2 - End Tidal Carbon Dioxide Fraction - unitless % roughly scaled to mmHg
    double BiogearsThread::GetEndTidalCarbonDioxideFraction() {
        return (m_pe->GetRespiratorySystem()->GetEndTidalCarbonDioxideFraction() * 762);

    }

// EtCO2 - End-Tidal Carbon Dioxide - mmHg
    double BiogearsThread::GetEndTidalCarbonDioxidePressure() {
        return m_pe->GetRespiratorySystem()->GetEndTidalCarbonDioxidePressure(biogears::PressureUnit::mmHg);
    }

// SPO2 - Oxygen Saturation - unitless %
    double BiogearsThread::GetOxygenSaturation() {
        return m_pe->GetBloodChemistrySystem()->GetOxygenSaturation() * 100;
    }

    double BiogearsThread::GetCarbonMonoxideSaturation() {
        return m_pe->GetBloodChemistrySystem()->GetCarbonMonoxideSaturation() * 100;

    }

    double BiogearsThread::GetInspiratoryFlow() {
        return m_pe->GetRespiratorySystem()->GetInspiratoryFlow(biogears::VolumePerTimeUnit::L_Per_min);
    }

    double BiogearsThread::GetRespiratoryTotalPressure() {
        return carina->GetSubstanceQuantity(*CO2)->GetPartialPressure(biogears::PressureUnit::cmH2O);
    }

    double BiogearsThread::GetPulmonaryResistance() {
        return m_pe->GetRespiratorySystem()->GetPulmonaryResistance(biogears::FlowResistanceUnit::cmH2O_s_Per_L);
    }

    double BiogearsThread::GetRawRespirationRate() {
        rawRespirationRate =
                m_pe->GetRespiratorySystem()->GetRespirationRate(biogears::FrequencyUnit::Per_min);
        return rawRespirationRate;
    }

// BR - Respiration Rate - per minute
    double BiogearsThread::GetRespirationRate() {
        double rr;
        double loss = GetBloodLossPercentage();
        if (m_pe->GetAnesthesiaMachine()->HasConnection() &&
            m_pe->GetAnesthesiaMachine()->GetConnection() != CDM::enumAnesthesiaMachineConnection::Off) {
            rr = rawRespirationRate;
        } else if (loss > 0.0) {
            rr = rawRespirationRate * (1 + 3 * std::max(0.0, loss - 0.2));
        } else {
            rr = rawRespirationRate;
        }
        return rr;
    }

// T2 - Core Temperature - degrees C
    double BiogearsThread::GetCoreTemperature() {
        return m_pe->GetEnergySystem()->GetCoreTemperature(biogears::TemperatureUnit::C);
    }

// ECG Waveform in mV
    double BiogearsThread::GetECGWaveform() {
        double ecgLead3_mV = m_pe->GetElectroCardioGram()->GetLead3ElectricPotential(
                biogears::ElectricPotentialUnit::mV);
        return ecgLead3_mV;
    }


    double BiogearsThread::GetOxyhemoglobinConcentration() {
        return HbO2->GetBloodConcentration(biogears::MassPerVolumeUnit::g_Per_dL);
    }

    double BiogearsThread::GetCarbaminohemoglobinConcentration() {
        return HbCO2->GetBloodConcentration(biogears::MassPerVolumeUnit::g_Per_dL);
    }

    double BiogearsThread::GetOxyCarbaminohemoglobinConcentration() {
        return HbO2CO2->GetBloodConcentration(biogears::MassPerVolumeUnit::g_Per_dL);
    }

    double BiogearsThread::GetCarboxyhemoglobinConcentration() {
        return HbCO->GetBloodConcentration(biogears::MassPerVolumeUnit::g_Per_dL);
    }

    double BiogearsThread::GetAnionGap() {
        return GetSodium() - (GetChloride() + GetBicarbonate());
    }

// Na+ - Sodium Concentration - mg/dL
    double BiogearsThread::GetSodiumConcentration() {
        return sodium->GetBloodConcentration(biogears::MassPerVolumeUnit::mg_Per_dL);
    }

// Na+ - Sodium - mmol/L
    double BiogearsThread::GetSodium() {
        return GetSodiumConcentration() * 0.43;
    }

// Glucose - Glucose Concentration - mg/dL
    double BiogearsThread::GetGlucoseConcentration() {
        return glucose->GetBloodConcentration(biogears::MassPerVolumeUnit::mg_Per_dL);
    }

// BUN - BloodUreaNitrogenConcentration - mg/dL
    double BiogearsThread::GetBUN() {
        return m_pe->GetBloodChemistrySystem()->GetBloodUreaNitrogenConcentration(
                biogears::MassPerVolumeUnit::mg_Per_dL);
    }

// Creatinine - Creatinine Concentration - mg/dL
    double BiogearsThread::GetCreatinineConcentration() {
        return creatinine->GetBloodConcentration(biogears::MassPerVolumeUnit::mg_Per_dL);
    }

    double BiogearsThread::GetCalciumConcentration() {
        return calcium->GetBloodConcentration(biogears::MassPerVolumeUnit::mg_Per_dL);
    }

    double BiogearsThread::GetIonizedCalcium() {
        double c1 = calcium->GetBloodConcentration(biogears::MassPerVolumeUnit::mg_Per_dL);
        return 0.45 * 0.2495 * c1;
    }

    double BiogearsThread::GetAlbuminConcentration() {
        return albumin->GetBloodConcentration(biogears::MassPerVolumeUnit::g_Per_dL);
    }

    double BiogearsThread::GetLactateConcentration() {
        lactateConcentration = lactate->GetBloodConcentration(biogears::MassPerVolumeUnit::g_Per_dL);
        return lactateConcentration;
    }

    double BiogearsThread::GetLactateConcentrationMMOL() {
        lactateMMOL = (lactateConcentration * 0.1110) * 1000;
        return lactateMMOL;
    }


    double BiogearsThread::GetTotalBilirubin() {
        return m_pe->GetBloodChemistrySystem()->GetTotalBilirubin(
                biogears::MassPerVolumeUnit::mg_Per_dL);
    }

    double BiogearsThread::GetTotalProtein() {
        biogears::SEComprehensiveMetabolicPanel metabolicPanel;
        m_pe->GetPatientAssessment(metabolicPanel);
        biogears::SEScalarMassPerVolume protein = metabolicPanel.GetTotalProtein();
        return protein.GetValue(biogears::MassPerVolumeUnit::g_Per_dL);
    }

// RBC - White Blood Cell Count - ct/uL
    double BiogearsThread::GetWhiteBloodCellCount() {
        return m_pe->GetBloodChemistrySystem()->GetWhiteBloodCellCount(
                biogears::AmountPerVolumeUnit::ct_Per_uL) /
               1000;
    }

// BC - Red Blood Cell Count - ct/uL
    double BiogearsThread::GetRedBloodCellCount() {
        return m_pe->GetBloodChemistrySystem()->GetRedBloodCellCount(
                biogears::AmountPerVolumeUnit::ct_Per_uL) /
               1000000;
    }

// Hgb - Hemoglobin Concentration - g/dL
    double BiogearsThread::GetHemoglobinConcentration() {
        return Hb->GetBloodConcentration(biogears::MassPerVolumeUnit::g_Per_dL);
    }

// Hct - Hematocrit - unitless
    double BiogearsThread::GetHematocrit() {
        return m_pe->GetBloodChemistrySystem()->GetHematocrit() * 100;
    }

// pH - Blood pH - unitless
    double BiogearsThread::GetRawBloodPH() {
        rawBloodPH = m_pe->GetBloodChemistrySystem()->GetVenousBloodPH();
        return rawBloodPH;
    }

    double BiogearsThread::GetModBloodPH() {
        return GetRawBloodPH() + 0.2 - 0.2 * sqrt(lactateMMOL);
    }

    double BiogearsThread::GetIntracranialPressure() {
        return m_pe->GetCardiovascularSystem()->GetIntracranialPressure(biogears::PressureUnit::mmHg);
    }

    double BiogearsThread::GetBloodPH() {
        bloodPH = rawBloodPH + 0.04 * std::min((1.5 - lactateMMOL), 0.0);
        return bloodPH;
    }

// PaCO2 - Arterial Carbon Dioxide Pressure - mmHg
    double BiogearsThread::GetArterialCarbonDioxidePressure() {
        return m_pe->GetBloodChemistrySystem()->GetArterialCarbonDioxidePressure(
                biogears::PressureUnit::mmHg);
    }

// Pa02 - Arterial Oxygen Pressure - mmHg
    double BiogearsThread::GetArterialOxygenPressure() {
        return m_pe->GetBloodChemistrySystem()->GetArterialOxygenPressure(biogears::PressureUnit::mmHg);
    }

    double BiogearsThread::GetVenousOxygenPressure() {
        return m_pe->GetBloodChemistrySystem()->GetVenousOxygenPressure(biogears::PressureUnit::mmHg);
    }

    double BiogearsThread::GetVenousCarbonDioxidePressure() {
        return m_pe->GetBloodChemistrySystem()->GetVenousCarbonDioxidePressure(
                biogears::PressureUnit::mmHg);
    }

// n/a - Bicarbonate Concentration - mg/L
    double BiogearsThread::GetBicarbonateConcentration() {
        return bicarbonate->GetBloodConcentration(biogears::MassPerVolumeUnit::mg_Per_dL);
    }

// HCO3 - Bicarbonate - Convert to mmol/L
    double BiogearsThread::GetBicarbonate() {
        return GetBicarbonateConcentration() * 0.1639;
    }

// BE - Base Excess -
    double BiogearsThread::GetBaseExcess() {
        return (0.93 * GetBicarbonate()) + (13.77 * GetModBloodPH()) - 124.58;
    }

    double BiogearsThread::GetBaseExcessRaw() {
            biogears::SEArterialBloodGasAnalysis abga;
            m_pe->GetPatientAssessment(abga);
            biogears::SEScalarAmountPerVolume be = abga.GetBaseExcess();
            return be.GetValue(biogears::AmountPerVolumeUnit::mmol_Per_L);
    }

    double BiogearsThread::GetBicarbonateRaw() {
        biogears::SEArterialBloodGasAnalysis abga;
        m_pe->GetPatientAssessment(abga);
        biogears::SEScalarAmountPerVolume bc = abga.GetStandardBicarbonate();
        return bc.GetValue(biogears::AmountPerVolumeUnit::mmol_Per_L);
    }

    double BiogearsThread::GetCO2() {
        biogears::SEComprehensiveMetabolicPanel metabolicPanel;
        m_pe->GetPatientAssessment(metabolicPanel);
        biogears::SEScalarAmountPerVolume CO2 = metabolicPanel.GetCO2();
        return CO2.GetValue(biogears::AmountPerVolumeUnit::mmol_Per_L);
    }

    double BiogearsThread::GetPotassium() {
        biogears::SEComprehensiveMetabolicPanel metabolicPanel;
        m_pe->GetPatientAssessment(metabolicPanel);
        biogears::SEScalarAmountPerVolume potassium = metabolicPanel.GetPotassium();
        return potassium.GetValue(biogears::AmountPerVolumeUnit::mmol_Per_L);
    }

    double BiogearsThread::GetChloride() {
        biogears::SEComprehensiveMetabolicPanel metabolicPanel;
        m_pe->GetPatientAssessment(metabolicPanel);
        biogears::SEScalarAmountPerVolume chloride = metabolicPanel.GetChloride();
        return chloride.GetValue(biogears::AmountPerVolumeUnit::mmol_Per_L);
    }

// PLT - Platelet Count - ct/uL
    double BiogearsThread::GetPlateletCount() {
        biogears::SECompleteBloodCount CBC;
        m_pe->GetPatientAssessment(CBC);
        biogears::SEScalarAmountPerVolume plateletCount = CBC.GetPlateletCount();
        return plateletCount.GetValue(biogears::AmountPerVolumeUnit::ct_Per_uL) / 1000;
    }

    double BiogearsThread::GetUrineProductionRate() {
        return m_pe->GetRenalSystem()->GetUrineProductionRate(biogears::VolumePerTimeUnit::mL_Per_min);
    }

    double BiogearsThread::GetUrineSpecificGravity() {
        return m_pe->GetRenalSystem()->GetUrineSpecificGravity();
    }

    double BiogearsThread::GetBladderGlucose() {
        return bladder->GetSubstanceQuantity(*glucose)->GetConcentration().GetValue(
                biogears::MassPerVolumeUnit::mg_Per_dL);
    }

    double BiogearsThread::GetShuntFraction() {
        return m_pe->GetBloodChemistrySystem()->GetShuntFraction();
    }

    double BiogearsThread::GetUrineOsmolality() {
        return m_pe->GetRenalSystem()->GetUrineOsmolality(biogears::OsmolalityUnit::mOsm_Per_kg);
    }

    double BiogearsThread::GetUrineOsmolarity() {
        return m_pe->GetRenalSystem()->GetUrineOsmolarity(biogears::OsmolarityUnit::mOsm_Per_L);
    }


// GetExhaledCO2 - tracheal CO2 partial pressure - mmHg
    double BiogearsThread::GetExhaledCO2() {
        return carina->GetSubstanceQuantity(*CO2)->GetPartialPressure(biogears::PressureUnit::mmHg);
    }

    double BiogearsThread::GetExhaledO2() {
        return carina->GetSubstanceQuantity(*O2)->GetPartialPressure(biogears::PressureUnit::mmHg);
    }

// Get Tidal Volume - mL
    double BiogearsThread::GetTidalVolume() {
        return m_pe->GetRespiratorySystem()->GetTidalVolume(biogears::VolumeUnit::mL);
    }

// Get Total Lung Volume - mL
    double BiogearsThread::GetTotalLungVolume() {
        return m_pe->GetRespiratorySystem()->GetTotalLungVolume(biogears::VolumeUnit::mL);
    }

// Get Left Lung Volume - mL
    double BiogearsThread::GetLeftLungVolume() {
        lung_vol_L = leftLung->GetVolume(biogears::VolumeUnit::mL);
        return lung_vol_L;
    }

// Get Right Lung Volume - mL
    double BiogearsThread::GetRightLungVolume() {
        lung_vol_R = rightLung->GetVolume(biogears::VolumeUnit::mL);
        return lung_vol_R;
    }

// Calculate and fetch left lung tidal volume
    double BiogearsThread::GetLeftLungTidalVolume() {
        if (falling_L) {
            if (lung_vol_L < new_min_L)
                new_min_L = lung_vol_L;
            else if (lung_vol_L > new_min_L + thresh) {
                falling_L = false;
                min_lung_vol_L = new_min_L;
                new_min_L = 1500.0;
                leftLungTidalVol = max_lung_vol_L - min_lung_vol_L;

                chestrise_pct_L =
                        leftLungTidalVol * 100 / 300; // scale tidal volume to percent of max chest rise
                if (chestrise_pct_L > 100)
                    chestrise_pct_L = 100; // clamp the value to 0 <= chestrise_pct <= 100
                if (chestrise_pct_L < 0)
                    chestrise_pct_L = 0;
            }
        } else {
            if (lung_vol_L > new_max_L)
                new_max_L = lung_vol_L;
            else if (lung_vol_L < new_max_L - thresh) {
                falling_L = true;
                max_lung_vol_L = new_max_L;
                new_max_L = 0.0;
                leftLungTidalVol = max_lung_vol_L - min_lung_vol_L;

                chestrise_pct_L =
                        leftLungTidalVol * 100 / 300; // scale tidal volume to percent of max chest rise
                if (chestrise_pct_L > 100)
                    chestrise_pct_L = 100; // clamp the value to 0 <= chestrise_pct <= 100
                if (chestrise_pct_L < 0)
                    chestrise_pct_L = 0;
            }
        }
        return leftLungTidalVol;
    }

// Calculate and fetch right lung tidal volume
    double BiogearsThread::GetRightLungTidalVolume() {
        if (falling_R) {
            if (lung_vol_R < new_min_R)
                new_min_R = lung_vol_R;
            else if (lung_vol_R > new_min_R + thresh) {
                falling_R = false;
                min_lung_vol_R = new_min_R;
                new_min_R = 1500.0;
                leftLungTidalVol = max_lung_vol_R - min_lung_vol_R;

                chestrise_pct_R =
                        rightLungTidalVol * 100 / 300; // scale tidal volume to percent of max chest rise
                if (chestrise_pct_R > 100)
                    chestrise_pct_R = 100; // clamp the value to 0 <= chestrise_pct <= 100
                if (chestrise_pct_R < 0)
                    chestrise_pct_R = 0;
            }
        } else {
            if (lung_vol_R > new_max_R)
                new_max_R = lung_vol_R;
            else if (lung_vol_R < new_max_R - thresh) {
                falling_R = true;
                max_lung_vol_R = new_max_R;
                new_max_R = 0.0;
                rightLungTidalVol = max_lung_vol_R - min_lung_vol_R;

                chestrise_pct_R =
                        rightLungTidalVol * 100 / 300; // scale tidal volume to percent of max chest rise
                if (chestrise_pct_R > 100)
                    chestrise_pct_R = 100; // clamp the value to 0 <= chestrise_pct <= 100
                if (chestrise_pct_R < 0)
                    chestrise_pct_R = 0;
            }
        }
        return rightLungTidalVol;
    }


// Get Left Lung Pleural Cavity Volume - mL
    double BiogearsThread::GetLeftPleuralCavityVolume() {
        return leftLung->GetVolume(biogears::VolumeUnit::mL);
    }

// Get Right Lung Pleural Cavity Volume - mL
    double BiogearsThread::GetRightPleuralCavityVolume() {
        return rightLung->GetVolume(biogears::VolumeUnit::mL);
    }

// Get left alveoli baseline compliance (?) volume
    double BiogearsThread::GetLeftAlveoliBaselineCompliance() {
        return leftLung->GetVolume(biogears::VolumeUnit::mL);
    }

// Get right alveoli baseline compliance (?) volume
    double BiogearsThread::GetRightAlveoliBaselineCompliance() {
        return rightLung->GetVolume(biogears::VolumeUnit::mL);
    }

    double BiogearsThread::GetCardiacOutput() {
        return m_pe->GetCardiovascularSystem()->GetCardiacOutput(
                biogears::VolumePerTimeUnit::mL_Per_min);
    }

    double BiogearsThread::GetPainVisualAnalogueScale() {
        return m_pe->GetNervousSystem()->GetPainVisualAnalogueScale();
    }

    void BiogearsThread::SetIVPump(const std::string &pumpSettings) {
        LOG_DEBUG << "Got pump settings: " << pumpSettings;
        std::string type, concentration, rate, dose, substance, bagVolume;
        std::vector <std::string> strings = Utility::explode("\n", pumpSettings);

        for (auto str : strings) {
            std::vector <std::string> strs;
            boost::split(strs, str, boost::is_any_of("="));
            auto strs_size = strs.size();
            // Check if it's not a key value pair
            if (strs_size != 2) {
                continue;
            }
            std::string kvp_k = strs[0];
            // all settings for the pump are strings
            std::string kvp_v = strs[1];

            try {
                if (kvp_k == "type") {
                    type = kvp_v;
                } else if (kvp_k == "substance") {
                    substance = kvp_v;
                } else if (kvp_k == "concentration") {
                    concentration = kvp_v;
                } else if (kvp_k == "rate") {
                    rate = kvp_v;
                } else if (kvp_k == "dose") {
                    dose = kvp_v;
                } else if (kvp_k == "amount") {
                    dose = kvp_v;
                } else if (kvp_k == "bagVolume") {
                    bagVolume = kvp_v;
                } else {
                    LOG_INFO << "Unknown pump setting: " << kvp_k << " = " << kvp_v;
                }
            }
            catch (std::exception &e) {
                LOG_ERROR << "Issue with setting " << e.what();
            }
        }

        if (substance == "Succinylcholine") {
            LOG_DEBUG << "Setting paralyzed to TRUE from succs infusion";
            paralyzed = true;
        }

        try {
            if (type == "infusion") {
                std::string concentrationsMass, concentrationsVol, rateUnit, massUnit, volUnit;
                double rateVal, massVal, volVal, conVal;

                if (substance == "Saline" || substance == "Whole Blood" || substance == "WholeBlood" ||
                    substance == "Antibiotic" ||
                    substance == "Blood" || substance == "RingersLactate" || substance == "PRBC"
                        ) {
                    if (substance == "Blood" || substance == "Whole Blood" || substance == "WholeBlood") {
                        substance = "Blood_ONegative";
                    }
                    biogears::SESubstanceCompound *subs =
                            m_pe->GetSubstanceManager().GetCompound(substance);
                    biogears::SESubstanceCompoundInfusion infuse(*subs);
                    std::vector <std::string> bagvol = Utility::explode(" ", bagVolume);
                    volVal = std::stod(bagvol[0]);
                    volUnit = bagvol[1];
                    LOG_DEBUG << "Setting bag volume to " << volVal << " / " << volUnit;
                    if (volUnit == "mL") {
                        infuse.GetBagVolume().SetValue(volVal, biogears::VolumeUnit::mL);
                    } else {
                        infuse.GetBagVolume().SetValue(volVal, biogears::VolumeUnit::L);
                    }

                    std::vector <std::string> rateb = Utility::explode(" ", rate);
                    rateVal = std::stod(rateb[0]);
                    rateUnit = rateb[1];

                    if (rateUnit == "mL/hr") {
                        LOG_DEBUG << "Infusing at " << rateVal << " mL per hour";
                        infuse.GetRate().SetValue(rateVal, biogears::VolumePerTimeUnit::mL_Per_hr);
                    } else {
                        LOG_DEBUG << "Infusing at " << rateVal << " mL per min";
                        infuse.GetRate().SetValue(rateVal, biogears::VolumePerTimeUnit::mL_Per_min);
                    }
                    m_pe->ProcessAction(infuse);
                } else {
                    biogears::SESubstance *subs = m_pe->GetSubstanceManager().GetSubstance(substance);
                    biogears::SESubstanceInfusion infuse(*subs);
                    std::vector <std::string> concentrations = Utility::explode("/", concentration);
                    concentrationsMass = concentrations[0];
                    concentrationsVol = concentrations[1];

                    std::vector <std::string> conmass = Utility::explode(" ", concentrationsMass);
                    massVal = std::stod(conmass[0]);
                    massUnit = conmass[1];
                    std::vector <std::string> convol = Utility::explode(" ", concentrationsVol);
                    volVal = std::stod(convol[0]);
                    volUnit = convol[1];
                    conVal = massVal / volVal;

                    LOG_DEBUG << "Infusing with concentration of " << conVal << " " << massUnit << "/"
                              << volUnit;

                    infuse.GetConcentration().SetValue(
                            conVal, biogears::MassPerVolumeUnit::mg_Per_mL);


                    std::vector <std::string> rateb = Utility::explode(" ", rate);
                    rateVal = std::stod(rateb[0]);
                    rateUnit = rateb[1];

                    if (rateUnit == "mL/hr") {
                        LOG_DEBUG << "Infusing at " << rateVal << " mL per hour";
                        infuse.GetRate().SetValue(rateVal, biogears::VolumePerTimeUnit::mL_Per_hr);
                    } else {
                        LOG_DEBUG << "Infusing at " << rateVal << " mL per min";
                        infuse.GetRate().SetValue(rateVal, biogears::VolumePerTimeUnit::mL_Per_min);
                    }
                    m_pe->ProcessAction(infuse);
                }
            } else if (type == "bolus") {
                const biogears::SESubstance *subs = m_pe->GetSubstanceManager().GetSubstance(substance);

                std::string concentrationsMass, concentrationsVol, massUnit, volUnit, doseUnit;
                double massVal, volVal, conVal, doseVal;
                std::vector <std::string> concentrations = Utility::explode("/", concentration);
                concentrationsMass = concentrations[0];
                concentrationsVol = concentrations[1];

                std::vector <std::string> conmass = Utility::explode(" ", concentrationsMass);
                massVal = std::stod(conmass[0]);
                massUnit = conmass[1];
                std::vector <std::string> convol = Utility::explode(" ", concentrationsVol);
                volVal = std::stod(convol[0]);
                volUnit = convol[1];
                conVal = massVal / volVal;

                std::vector <std::string> doseb = Utility::explode(" ", dose);
                doseVal = std::stod(doseb[0]);
                doseUnit = doseb[1];

                biogears::SESubstanceBolus bolus(*subs);
                LOG_DEBUG << "Bolus with concentration of " << conVal << " " << massUnit << "/"
                          << volUnit;
                if (massUnit == "mg" && volUnit == "mL") {
                    bolus.GetConcentration().SetValue(conVal, biogears::MassPerVolumeUnit::mg_Per_mL);
                } else {
                    bolus.GetConcentration().SetValue(conVal, biogears::MassPerVolumeUnit::ug_Per_mL);
                }
                LOG_DEBUG << "Bolus with a dose of  " << doseVal << doseUnit;
                if (doseUnit == "mL") {
                    bolus.GetDose().SetValue(doseVal, biogears::VolumeUnit::mL);
                } else {
                    bolus.GetDose().SetValue(doseVal, biogears::VolumeUnit::uL);
                }


                // IV pump only uses IV administration for right now
                bolus.SetAdminRoute(CDM::enumBolusAdministration::Intravenous);

                m_pe->ProcessAction(bolus);
            }
        }
        catch (std::exception &e) {
            LOG_ERROR << "Error processing ivpump action: " << e.what();
        }
    }

    void BiogearsThread::SetSubstanceInfusion(const std::string &substance, double conVal,
                                              const std::string &conUnit, double rate,
                                              const std::string &rUnit) {
        try {
            biogears::SESubstance *subs = m_pe->GetSubstanceManager().GetSubstance(substance);
            biogears::SESubstanceInfusion infuse(*subs);

            LOG_DEBUG << "Infusing with concentration of " << conVal << " " << conUnit;

            // @TODO: Parse out concentration unit
            infuse.GetConcentration().SetValue(
                    conVal, biogears::MassPerVolumeUnit::mg_Per_mL);

            // @TODO: Parse out rate unit
            if (rUnit == "mL/hr") {
                LOG_DEBUG << "Infusing at " << rate << " mL per hour";
                infuse.GetRate().SetValue(rate, biogears::VolumePerTimeUnit::mL_Per_hr);
            } else {
                LOG_DEBUG << "Infusing at " << rate << " mL per min";
                infuse.GetRate().SetValue(rate, biogears::VolumePerTimeUnit::mL_Per_min);
            }
            m_pe->ProcessAction(infuse);
        } catch (std::exception &e) {
            LOG_ERROR << "Error processing substance bolus action: " << e.what();
        }
        return;
    }

    void BiogearsThread::SetSubstanceCompoundInfusion(const std::string &substance, double bagVolume,
                                                      const std::string &bvUnit, double rate,
                                                      const std::string &rUnit) {
        try {
            if (substance == "Blood" || substance == "Whole Blood" || substance == "WholeBlood") {
                substance == "Blood_ONegative";
            }

            biogears::SESubstanceCompound *subs =
                    m_pe->GetSubstanceManager().GetCompound(substance);
            biogears::SESubstanceCompoundInfusion infuse(*subs);

            LOG_DEBUG << "Setting bag volume to " << bagVolume << " / " << bvUnit;
            if (bvUnit == "mL") {
                infuse.GetBagVolume().SetValue(bagVolume, biogears::VolumeUnit::mL);
            } else {
                infuse.GetBagVolume().SetValue(bagVolume, biogears::VolumeUnit::L);
            }

            if (rUnit == "mL/hr") {
                LOG_DEBUG << "Infusing at " << rate << " mL per hour";
                infuse.GetRate().SetValue(rate, biogears::VolumePerTimeUnit::mL_Per_hr);
            } else {
                LOG_DEBUG << "Infusing at " << rate << " mL per min";
                infuse.GetRate().SetValue(rate, biogears::VolumePerTimeUnit::mL_Per_min);
            }
            m_pe->ProcessAction(infuse);
        } catch (std::exception &e) {
            LOG_ERROR << "Error processing substance bolus action: " << e.what();
        }
        return;
    }

    void BiogearsThread::SetSubstanceNasalDose(const std::string &substance, double dose,
                                               const std::string &doseUnit) {
        try {
            const biogears::SESubstance *subs = m_pe->GetSubstanceManager().GetSubstance(substance);
            biogears::SESubstanceNasalDose nd(*subs);
            LOG_DEBUG << "Nasally administered substance with a dose of  " << dose << doseUnit;
            if (doseUnit == "mg") {
               nd.GetDose().SetValue(dose, biogears::MassUnit::mg);
            } else {
                nd.GetDose().SetValue(dose, biogears::MassUnit::g);
            }
            m_pe->ProcessAction(nd);
        } catch (std::exception &e) {
            LOG_ERROR << "Error processing substance nasal dose action: " << e.what();
        }
        return;
    }

    void BiogearsThread::SetSubstanceBolus(const std::string &substance, double concentration,
                                           const std::string &concUnit, double dose,
                                           const std::string &doseUnit, const std::string &adminRoute) {
        try {
            const biogears::SESubstance *subs = m_pe->GetSubstanceManager().GetSubstance(substance);
            biogears::SESubstanceBolus bolus(*subs);
            LOG_DEBUG << "Bolus with concentration of " << concentration << " " << concUnit;
            if (concUnit == "mg/mL") {
                bolus.GetConcentration().SetValue(concentration, biogears::MassPerVolumeUnit::mg_Per_mL);
            } else {
                bolus.GetConcentration().SetValue(concentration, biogears::MassPerVolumeUnit::ug_Per_mL);
            }
            LOG_DEBUG << "Bolus with a dose of  " << dose << doseUnit;
            if (doseUnit == "mL") {
                bolus.GetDose().SetValue(dose, biogears::VolumeUnit::mL);
            } else {
                bolus.GetDose().SetValue(dose, biogears::VolumeUnit::uL);
            }

            CDM::enumBolusAdministration aRoute;
            std::string lAR = boost::algorithm::to_lower_copy(adminRoute);
            if (lAR == "intraarterial") {
                aRoute = CDM::enumBolusAdministration::Intraarterial;
            } else if (lAR == "intramuscular") {
                aRoute = CDM::enumBolusAdministration::Intramuscular;
            } else {
                aRoute = CDM::enumBolusAdministration::Intravenous;
            }

            bolus.SetAdminRoute(aRoute);

            if (substance == "Succinylcholine") {
                LOG_DEBUG << "Setting paralyzed to TRUE from succs infusion";
                paralyzed = true;
            }

            m_pe->ProcessAction(bolus);
        } catch (std::exception &e) {
            LOG_ERROR << "Error processing substance bolus action: " << e.what();
        }
        return;
    }

    void BiogearsThread::SetAirwayObstruction(double severity) {
        try {
            biogears::SEAirwayObstruction obstruction;
            obstruction.GetSeverity().SetValue(severity);
            m_pe->ProcessAction(obstruction);
        }
        catch (std::exception &e) {
            LOG_ERROR << "Error processing airway obstruction action: " << e.what();
        }
    }

    void BiogearsThread::SetAsthmaAttack(double severity) {
        try {
            SEAsthmaAttack asthmaAttack;
            asthmaAttack.GetSeverity().SetValue(severity);
            m_pe->ProcessAction(asthmaAttack);
        }
        catch (std::exception &e) {
            LOG_ERROR << "Error processing asthma action: " << e.what();
        }
    }

    void BiogearsThread::SetBrainInjury(double severity, const std::string &type) {
        try {
            SEBrainInjury tbi;
            if (type == "Diffuse") {
                tbi.SetType(CDM::enumBrainInjuryType::Diffuse);
            } else if (type == "LeftFocal") {
                tbi.SetType(CDM::enumBrainInjuryType::LeftFocal);
            } else if (type == "RightFocal") {
                tbi.SetType(CDM::enumBrainInjuryType::RightFocal);
            }
            tbi.GetSeverity().SetValue(severity);
            m_pe->ProcessAction(tbi);
        }
        catch (std::exception &e) {
            LOG_ERROR << "Error processing brain injury action: " << e.what();
        }
    }

    void BiogearsThread::SetHemorrhage(const std::string &location, double flow) {
        try {
            biogears::SEHemorrhage hemorrhage;
            hemorrhage.SetCompartment(location);
            hemorrhage.GetInitialRate().SetValue(flow, biogears::VolumePerTimeUnit::mL_Per_min);
            hemorrhage.SetMCIS();
            m_pe->ProcessAction(hemorrhage);
            //m_mutex.lock();
            //if (!) {
//                LOG_ERROR << "Unable to process action.";
//            }
            //m_mutex.unlock();
        } catch (std::exception &e) {
            LOG_ERROR << "Error processing hemorrhage action: " << e.what();
        }
    }

    void BiogearsThread::SetNeedleDecompression(const std::string &location) {

    }

    void BiogearsThread::SetPain(const std::string &location, double severity) {
        try {
            biogears::SEPainStimulus PainStimulus;
            PainStimulus.SetLocation(location);
            PainStimulus.GetSeverity().SetValue(severity);
            m_pe->ProcessAction(PainStimulus);
        }
        catch (std::exception &e) {
            LOG_ERROR << "Error processing pain action: " << e.what();
        }
    }


    void BiogearsThread::SetSepsis(const std::string &location, double severity) {
        try {
//          biogears::SESepsisState sepsis;
//          PainStimulus.SetLocation(location);
//          PainStimulus.GetSeverity().SetValue(severity);
//          m_pe->ProcessAction(PainStimulus);
        }
        catch (std::exception &e) {
            LOG_ERROR << "Error processing pain action: " << e.what();
        }
    }


    bool BiogearsThread::ExecuteCommand(const std::string &cmd) {
        std::string scenarioFile = "Actions/" + cmd + ".xml";
        return LoadScenarioFile(scenarioFile);
    }

    void BiogearsThread::SetVentilator(const std::string &ventilatorSettings) {
        std::vector <std::string> strings = Utility::explode("\n", ventilatorSettings);

        biogears::SEAnesthesiaMachineConfiguration AMConfig(m_pe->GetSubstanceManager());
        biogears::SEAnesthesiaMachine &config = AMConfig.GetConfiguration();

        config.GetInletFlow().SetValue(2.0, biogears::VolumePerTimeUnit::L_Per_min);
        config.SetPrimaryGas(CDM::enumAnesthesiaMachinePrimaryGas::Nitrogen);
        config.SetConnection(CDM::enumAnesthesiaMachineConnection::Tube);
        config.SetOxygenSource(CDM::enumAnesthesiaMachineOxygenSource::Wall);
        config.GetReliefValvePressure().SetValue(20.0, biogears::PressureUnit::cmH2O);

        for (auto str : strings) {
            std::vector <std::string> strs;
            boost::split(strs, str, boost::is_any_of("="));
            auto strs_size = strs.size();
            // Check if it's not a key value pair
            if (strs_size != 2) {
                continue;
            }
            std::string kvp_k = strs[0];
            // all settings for the ventilator are floats
            double kvp_v = std::stod(strs[1]);
            try {
                if (kvp_k == "OxygenFraction") {
                    config.GetOxygenFraction().SetValue(kvp_v);
                } else if (kvp_k == "PositiveEndExpiredPressure") {
                    config.GetPositiveEndExpiredPressure().SetValue(
                            kvp_v * 100, biogears::PressureUnit::cmH2O);
                } else if (kvp_k == "RespiratoryRate") {
                    config.GetRespiratoryRate().SetValue(kvp_v, biogears::FrequencyUnit::Per_min);
                } else if (kvp_k == "InspiratoryExpiratoryRatio") {
                    config.GetInspiratoryExpiratoryRatio().SetValue(kvp_v);
                } else if (kvp_k == "TidalVolume") {
                    // empty
                } else if (kvp_k == "VentilatorPressure") {
                    config.GetVentilatorPressure().SetValue(kvp_v, biogears::PressureUnit::cmH2O);
                } else if (kvp_k == " ") {
                    // empty
                } else {
                    LOG_INFO << "Unknown ventilator setting: " << kvp_k << " = " << kvp_v;
                }
            }
            catch (std::exception &e) {
                LOG_ERROR << "Issue with setting " << e.what();
            }
        }

        try {
            m_pe->ProcessAction(AMConfig);
        }
        catch (std::exception &e) {
            LOG_ERROR << "Error processing ventilator action: " << e.what();
        }
    }

    void BiogearsThread::SetBVMMask(const std::string &ventilatorSettings) {
        std::vector <std::string> strings = Utility::explode("\n", ventilatorSettings);

        biogears::SEAnesthesiaMachineConfiguration AMConfig(m_pe->GetSubstanceManager());
        biogears::SEAnesthesiaMachine &config = AMConfig.GetConfiguration();

        config.GetInletFlow().SetValue(2.0, biogears::VolumePerTimeUnit::L_Per_min);
        config.SetPrimaryGas(CDM::enumAnesthesiaMachinePrimaryGas::Air);
        config.SetConnection(CDM::enumAnesthesiaMachineConnection::Mask);
        config.SetOxygenSource(CDM::enumAnesthesiaMachineOxygenSource::Wall);
        config.GetReliefValvePressure().SetValue(20.0, biogears::PressureUnit::cmH2O);

        for (auto str : strings) {
            std::vector <std::string> strs;
            boost::split(strs, str, boost::is_any_of("="));
            auto strs_size = strs.size();
            // Check if it's not a key value pair
            if (strs_size != 2) {
                continue;
            }
            std::string kvp_k = strs[0];
            // all settings for the ventilator are floats
            double kvp_v = std::stod(strs[1]);
            try {
                if (kvp_k == "OxygenFraction") {
                    config.GetOxygenFraction().SetValue(kvp_v);
                } else if (kvp_k == "PositiveEndExpiredPressure") {
                    config.GetPositiveEndExpiredPressure().SetValue(
                            kvp_v * 100, biogears::PressureUnit::cmH2O);
                } else if (kvp_k == "RespiratoryRate") {
                    config.GetRespiratoryRate().SetValue(kvp_v, biogears::FrequencyUnit::Per_min);
                } else if (kvp_k == "InspiratoryExpiratoryRatio") {
                    config.GetInspiratoryExpiratoryRatio().SetValue(kvp_v);
                } else if (kvp_k == "TidalVolume") {
                    // empty
                } else if (kvp_k == "VentilatorPressure") {
                    config.GetVentilatorPressure().SetValue(kvp_v, biogears::PressureUnit::cmH2O);
                } else if (kvp_k == " ") {
                    // empty
                } else {
                    LOG_INFO << "Unknown BVM setting: " << kvp_k << " = " << kvp_v;
                }
            }
            catch (std::exception &e) {
                LOG_ERROR << "Issue with setting " << e.what();
            }
        }

        try {
            m_pe->ProcessAction(AMConfig);
        }
        catch (std::exception &e) {
            LOG_ERROR << "Error processing BVM action: " << e.what();
        }
    }

    // The Glasgow Coma Scale (GCS) is commonly used to classify patient consciousness after traumatic brain injury.
// The scale runs from 3 to 15, with brain injury classified as:
// Severe (GCS < 9)
// Moderate (GCS 9-12)
// Mild (GCS > 12)
    int BiogearsThread::GlasgowEstimator(double cbf) {
        if (cbf < 116)
            return 3;
        else if (cbf < 151)
            return 4;
        else if (cbf < 186)
            return 5;
        else if (cbf < 220)
            return 6;
        else if (cbf < 255)
            return 7;
        else if (cbf < 290)
            return 8;
        else if (cbf < 363)
            return 9;
        else if (cbf < 435)
            return 10;
        else if (cbf < 508)
            return 11;
        else if (cbf < 580)
            return 12;
        else if (cbf < 628)
            return 13;
        else if (cbf < 725)
            return 14;
        else
            return 15;
    }

    void BiogearsThread::Status() {
        LOG_INFO << "-------------------------";
        //LOG_INFO << "Biogears Version: " << mil::tatrc::physiology::biogears::Version;
        //LOG_INFO << "-------------------------";

        if (running) {
            LOG_INFO << "Running:\t\t\t\tTrue";
            LOG_INFO
                    << "Simulation Time:\t\t" << m_pe->GetSimulationTime(biogears::TimeUnit::s)
                    << "s";
            LOG_INFO
                    << "Cardiac Output:\t\t\t"
                    << m_pe->GetCardiovascularSystem()->GetCardiacOutput(
                            biogears::VolumePerTimeUnit::mL_Per_min);// << biogears::VolumePerTimeUnit::mL_Per_min;
            LOG_INFO
                    << "Blood Volume:\t\t\t"
                    << m_pe->GetCardiovascularSystem()->GetBloodVolume(
                            biogears::VolumeUnit::mL);// << biogears::VolumeUnit::mL;
            LOG_INFO
                    << "Mean Arterial Pressure:\t"
                    << m_pe->GetCardiovascularSystem()->GetMeanArterialPressure(
                            biogears::PressureUnit::mmHg);// << biogears::PressureUnit::mmHg;
            LOG_INFO
                    << "Systolic Pressure:\t\t"
                    << m_pe->GetCardiovascularSystem()->GetSystolicArterialPressure(
                            biogears::PressureUnit::mmHg);// << biogears::PressureUnit::mmHg;
            LOG_INFO
                    << "Diastolic Pressure:\t\t"
                    << m_pe->GetCardiovascularSystem()->GetDiastolicArterialPressure(
                            biogears::PressureUnit::mmHg);// << biogears::PressureUnit::mmHg;
            LOG_INFO
                    << "Heart Rate:\t\t\t\t"
                    << m_pe->GetCardiovascularSystem()->GetHeartRate(
                            biogears::FrequencyUnit::Per_min)
                    << "bpm";
            LOG_INFO
                    << "Respiration Rate:\t\t"
                    << m_pe->GetRespiratorySystem()->GetRespirationRate(
                            biogears::FrequencyUnit::Per_min)
                    << "bpm";
        } else {
            LOG_INFO << "Running:\t\t\t\tFalse";
        }
    }

    double BiogearsThread::GetCerebralPerfusionPressure() {
        return m_pe->GetCardiovascularSystem()->GetCerebralPerfusionPressure(PressureUnit::mmHg);
    }

    double BiogearsThread::GetCerebralBloodFlow() {
        return m_pe->GetCardiovascularSystem()->GetCerebralBloodFlow(VolumePerTimeUnit::mL_Per_min);
    }

    double BiogearsThread::GetPatientAge() {
        return 0;
    }

    double BiogearsThread::GetPatientWeight() {
        return 0;
    }

    double BiogearsThread::GetPatientGender() {
        return 0;
    }

    double BiogearsThread::GetPatientHeight() {
        return 0;
    }

    double BiogearsThread::GetPatient_BodyFatFraction() {
        return 0;
    }

    double BiogearsThread::GetGCSValue() {
        return GlasgowEstimator(GetCerebralBloodFlow());
    }
}
